#pragma once

#include "dbg.h"
#include "epoll.h"
#include "../context/context.h"

#include "co/co.h"
#include "co/def.h"
#include "co/flag.h"
#include "co/log.h"
#include "co/atomic.h"
#include "co/fastream.h"
#include "co/closure.h"
#include "co/thread.h"
#include "co/time.h"

#include <assert.h>
#include <string.h>
#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <unordered_map>

DEC_uint32(co_sched_num);
DEC_uint32(co_stack_size);

namespace co {

struct Coroutine;
typedef std::multimap<int64, Coroutine*>::iterator timer_id_t;
extern const timer_id_t null_timer_id;

class Scheduler;
extern __thread Scheduler* gSched;

// coroutine status
//   co::Event::wait()    ->  S_wait
//   co::Event::signal()  ->  S_ready
enum _CoState {
    S_init = 0,    // initial state
    S_wait = 1,    // wait for a event
    S_ready = 2,   // ready to resume
};

struct Coroutine {
    explicit Coroutine(int coid)
        : id(coid), state(S_init), ctx(0), stack(), cb(0) {
    }

    ~Coroutine() = default;

    int id;           // coroutine id
    int state;        // coroutine state
    tb_context_t ctx; // context, a pointer points to the stack bottom
    fastream stack;   // save stack data for this coroutine
    union {
        Closure* cb;  // coroutine function
        Scheduler* s; // scheduler this coroutines runs in
    };
};

// pool for coroutines, using index as the coroutine id.
class Copool {
  public:
    Copool() {
        _pool.reserve(1024);
        _ids.reserve(1024);
    }

    ~Copool() {
        for (size_t i = 0; i < _pool.size(); ++i) delete _pool[i];
    }

    Coroutine* pop() {
        if (!_ids.empty()) {
            Coroutine* co = _pool[_ids.back()];
            _ids.pop_back();
            return co;
        } else {
            Coroutine* co = new Coroutine((int) _pool.size());
            _pool.push_back(co);
            return co;
        }
    }

    void push(Coroutine* co) {
        _ids.push_back(co->id);
    }

    Coroutine* operator[](size_t i) const {
        return _pool[i];
    }

  private:
    std::vector<Coroutine*> _pool; // coroutine pool
    std::vector<int> _ids;         // id of available coroutines in _pool
};

class TaskManager {
  public:
    TaskManager() = default;
    ~TaskManager() = default;

    void add_task(Closure* cb) {
        ::MutexGuard g(_mtx);
       _task_cb.push_back(cb);
    }

    void add_task(Coroutine* co) {
        ::MutexGuard g(_mtx);
        _task_co.push_back(co);
    }

    void get_tasks(std::vector<Closure*>& task_cb, std::vector<Coroutine*>& task_co) {
        ::MutexGuard g(_mtx);
        if (!_task_cb.empty()) _task_cb.swap(task_cb);
        if (!_task_co.empty()) _task_co.swap(task_co);
    }
 
  private:
    ::Mutex _mtx;
    std::vector<Closure*> _task_cb;   // newly added tasks
    std::vector<Coroutine*> _task_co; // tasks ready to resume
};

class TimerManager {
  public:
    TimerManager() : _timer() {
        _it = _timer.end();
    }

    ~TimerManager() = default;

    // for sleep, co::Event...
    timer_id_t add_timer(uint32 ms, Coroutine* co) {
        return _timer.insert(std::make_pair(now::ms() + ms, co));
    }

    // for co::IoEvent
    timer_id_t add_io_timer(uint32 ms, Coroutine* co) {
        return _it = _timer.insert(_it, std::make_pair(now::ms() + ms, co));
    }

    void del_timer(const timer_id_t& id) {
        if (_it == id) ++_it;
        _timer.erase(id);
    }

    void add_task(Coroutine* co, const timer_id_t& id) {
        ::MutexGuard g(_mtx);
        _timer_tasks.insert(std::make_pair(co, id));
    }

    void get_tasks(std::unordered_map<Coroutine*, timer_id_t>& tasks) {
        ::MutexGuard g(_mtx);
        if (!_timer_tasks.empty()) _timer_tasks.swap(tasks);
    }

    // return time until next timeout
    uint32 check_timeout(std::vector<Coroutine*>& res);

  private:
    std::multimap<int64, Coroutine*> _timer;        // timed-wait tasks: <time_ms, co>
    std::multimap<int64, Coroutine*>::iterator _it; // make insert faster with this hint

    ::Mutex _mtx;
    std::unordered_map<Coroutine*, timer_id_t> _timer_tasks; // timer tasks ready to resume
};

class Scheduler {
  public:
    Scheduler(uint32 id, uint32 stack_size);
    ~Scheduler();

    uint32 id() const { return _id; }

    Coroutine* running() const { return _running; }

    void resume(Coroutine* co);

    void yield() {
        tb_context_jump(_main_co->ctx, _running);
    }

    void add_task(Closure* cb) {
        _task_mgr.add_task(cb);
        _epoll.signal();
    }

    void add_task(Coroutine* co) {
        _task_mgr.add_task(co);
        _epoll.signal();
    }

    void add_task(Coroutine* co, const timer_id_t& id) {
        _timer_mgr.add_task(co, id);
        _epoll.signal();
    }

    void sleep(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        auto it = _timer_mgr.add_timer(ms, _running);
        COLOG << "sleep " << ms << " ms, add timer: " << COTID(it);
        this->yield();
    }

    timer_id_t add_timer(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        auto it = _timer_mgr.add_timer(ms, _running);
        COLOG << "add timer" << "(" << ms << " ms): " << COTID(it);
        return it;
    }

    timer_id_t add_io_timer(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        auto it = _timer_mgr.add_io_timer(ms, _running);
        COLOG << "add io timer" << "(" << ms << " ms): " << COTID(it);
        return it;
    }

    void del_timer(const timer_id_t& id) {
        COLOG << "del timer: " << COTID(id);
        _timer_mgr.del_timer(id);
    }

    void loop();

    void loop_in_thread() {
        Thread(&Scheduler::loop, this).detach();
    }

    void stop();

  #if defined(_WIN32)
    bool add_event(sock_t fd, int ev) {
        return _epoll.add_event(fd, ev);
    }
  #elif defined(__linux__)
    bool add_event(sock_t fd, int ev) {
        return _epoll.add_event(fd, ev, _running->id);
    }
  #else
    bool add_event(sock_t fd, int ev) {
        return _epoll.add_event(fd, ev, _running);
    }
  #endif

    void del_event(sock_t fd, int ev) {
        _epoll.del_event(fd, ev);
    }

    void del_event(sock_t fd) {
        _epoll.del_event(fd);
    }

    void recycle(Coroutine* co) {
        _co_pool.push(co);
    }

    bool timeout() const { return _timeout; }

    bool on_stack(void* p) const {
        return (_stack <= (char*)p) && ((char*)p < _stack + _stack_size);
    }

    // the cleanup callbacks are called at the end of loop()
    void add_cleanup_cb(std::function<void()>&& cb) {
        _cbs.push_back(std::move(cb));
    }

  private:
    void save_stack(Coroutine* co) {
        co->stack.clear();
        co->stack.append(co->ctx, _stack + _stack_size - (char*)co->ctx);
    }

    Coroutine* new_coroutine(Closure* cb) {
        Coroutine* co = _co_pool.pop();
        co->cb = cb;
        return co;
    }

    void cleanup() {
        for (size_t i = 0; i < _cbs.size(); ++i) _cbs[i]();
        _cbs.clear();
    }

  private:
    uint32 _id;          // scheduler id   
    uint32 _stack_size;  // size of stack
    char* _stack;        // stack shared by coroutines in this scheduler
    Coroutine* _main_co; // save the main context
    Coroutine* _running; // the current running coroutine
    Epoll _epoll;
    uint32 _wait_ms;     // time epoll to wait

    Copool _co_pool;
    TaskManager _task_mgr;
    TimerManager _timer_mgr;
    std::vector<std::function<void()>> _cbs;

    SyncEvent _ev;
    bool _stop;
    bool _timeout;
};

class SchedManager {
  public:
    SchedManager();
    ~SchedManager();

    // return next scheduler
    Scheduler* next() {
        if (_s != (uint32)-1) return _scheds[atomic_inc(&_n) & _s];
        uint32 n = atomic_inc(&_n);
        if (n <= ~_r) return _scheds[n % _scheds.size()]; // n <= (2^32 - 1 - r)
        return _scheds[now::us() % _scheds.size()];
    }

    // stop all schedulers
    void stop();

  private:
    std::vector<Scheduler*> _scheds;
    uint32 _n;  // index, initialized as -1
    uint32 _r;  // 2^32 % sched_num
    uint32 _s;  // _r = 0, _s = sched_num-1;  _r != 0, _s = -1;
};

inline SchedManager* sched_mgr() {
    static SchedManager kSchedMgr;
    return &kSchedMgr;
}

} // co
