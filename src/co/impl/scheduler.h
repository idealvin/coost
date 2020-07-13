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

inline fastream& operator<<(fastream& fs, const timer_id_t& id) {
    return fs << *(void**)(&id);
}

// coroutine status
//   co::Event::wait()    ->  S_wait
//   co::Event::signal()  ->  S_ready
enum _CoState {
    S_init = 0,    // initial state
    S_wait = 1,    // wait for a event
    S_ready = 2,   // ready to resume
};

struct Coroutine {
    explicit Coroutine(int i)
        : id(i), state(S_init), ctx(0), stack(), cb(0) {
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

// pool of Coroutine, using index as the coroutine id.
class Copool {
  public:
    Copool(size_t cap=1024) {
        _pool.reserve(cap);
        _ids.reserve(cap);
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

// Tasks may be added from any thread. We need a Mutex here.
class TaskManager {
  public:
    TaskManager() = default;
    ~TaskManager() = default;

    void add_new_task(Closure* cb) {
        ::MutexGuard g(_mtx);
       _new_tasks.push_back(cb);
    }

    void add_ready_task(Coroutine* co) {
        ::MutexGuard g(_mtx);
        _ready_tasks.push_back(co);
    }

    void add_ready_timer_task(Coroutine* co, const timer_id_t& id) {
        ::MutexGuard g(_mtx);
        _ready_timer_tasks.insert(std::make_pair(co, id));
    }

    void get_all_tasks(
        std::vector<Closure*>& new_tasks,
        std::vector<Coroutine*>& ready_tasks,
        std::unordered_map<Coroutine*, timer_id_t>& ready_timer_tasks
    ) {
        ::MutexGuard g(_mtx);
        if (!_new_tasks.empty()) _new_tasks.swap(new_tasks);
        if (!_ready_tasks.empty()) _ready_tasks.swap(ready_tasks);
        if (!_ready_timer_tasks.empty()) _ready_timer_tasks.swap(ready_timer_tasks);
    }
 
  private:
    ::Mutex _mtx;
    std::vector<Closure*> _new_tasks;   
    std::vector<Coroutine*> _ready_tasks;
    std::unordered_map<Coroutine*, timer_id_t> _ready_timer_tasks;
};

// Timer must be added in the scheduler thread. We need no lock here.
class TimerManager {
  public:
    TimerManager() : _timer(), _it(_timer.end()) {}
    ~TimerManager() = default;

    timer_id_t add_timer(uint32 ms, Coroutine* co) {
        return _timer.insert(std::make_pair(now::ms() + ms, co));
    }

    timer_id_t add_io_timer(uint32 ms, Coroutine* co) {
        return _it = _timer.insert(_it, std::make_pair(now::ms() + ms, co));
    }

    void del_timer(const timer_id_t& id) {
        if (_it == id) ++_it;
        _timer.erase(id);
    }

    // return time(ms) to wait for the next timeout.
    // all timedout coroutine will be pushed into @res.
    uint32 check_timeout(std::vector<Coroutine*>& res);

    // ========================================================================
    // for unitest/co
    // ========================================================================
    bool assert_empty() const { return _timer.empty() && _it == _timer.end(); }
    bool assert_it(const timer_id_t& it) const { return _it == it; }

  private:
    std::multimap<int64, Coroutine*> _timer;        // timed-wait tasks: <time_ms, co>
    std::multimap<int64, Coroutine*>::iterator _it; // make insert faster with this hint
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

    // We must reset the Coroutine here.
    void recycle(Coroutine* co) {
        assert(co->state == S_init);
        co->stack.clear();
        co->ctx = 0;
        _co_pool.push(co);
    }

    void loop();

    void loop_in_thread() {
        Thread(&Scheduler::loop, this).detach();
    }

    void stop();

    bool timeout() const { return _timeout; }

    bool on_stack(void* p) const {
        return (_stack <= (char*)p) && ((char*)p < _stack + _stack_size);
    }

    // these callbacks are called at the end of loop()
    void add_cleanup_cb(std::function<void()>&& cb) {
        _cbs.push_back(std::move(cb));
    }

    // =========================================================================
    // add task ready to resume
    // =========================================================================
    // for go(...)
    void add_new_task(Closure* cb) {
        _task_mgr.add_new_task(cb);
        _epoll.signal();
    }

    // Mutex::unlock()
    // Event::signal() to Event::wait()
    void add_ready_task(Coroutine* co) {
        _task_mgr.add_ready_task(co);
        _epoll.signal();
    }

    // Event::signal() to Event::wait(ms)   (co->state: S_wait -> S_ready)
    void add_ready_timer_task(Coroutine* co, const timer_id_t& id) {
        assert(id != null_timer_id);
        COLOG << "add ready timer task, S" << co->s->id() << '.' << co->id << ", timer_id: " << id;
        _task_mgr.add_ready_timer_task(co, id);
        _epoll.signal();
    }

    // =========================================================================
    // add or delete a timer
    // =========================================================================
    void sleep(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        auto it = _timer_mgr.add_timer(ms, _running);
        COLOG << "sleep " << ms << " ms, add timer: " << it;
        this->yield();
    }

    // for Event::wait(ms)
    timer_id_t add_timer(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        auto it = _timer_mgr.add_timer(ms, _running);
        COLOG << "add timer: " << it << " (" << ms << " ms)";
        return it;
    }

    // for IoEvent
    timer_id_t add_io_timer(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        auto it = _timer_mgr.add_io_timer(ms, _running);
        COLOG << "add io timer: " << it << " (" << ms << " ms)";
        return it;
    }

    void del_timer(const timer_id_t& id) {
        COLOG << "del timer: " << id;
        _timer_mgr.del_timer(id);
    }

    // =========================================================================
    // add or delete io event
    // =========================================================================
  #if defined(_WIN32)
    bool add_event(sock_t fd) {
        return _epoll.add_event(fd);
    }
  #elif defined(__linux__)
    bool add_event(sock_t fd, int ev) {
        return _epoll.add_event(fd, ev, _running->id);
    }

    void del_event(sock_t fd, int ev) {
        _epoll.del_event(fd, ev);
    }
  #else
    bool add_event(sock_t fd, int ev) {
        return _epoll.add_event(fd, ev, _running);
    }

    void del_event(sock_t fd, int ev) {
        _epoll.del_event(fd, ev);
    }
  #endif

    void del_event(sock_t fd) {
        _epoll.del_event(fd);
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
