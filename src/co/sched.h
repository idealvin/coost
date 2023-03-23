#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4127)
#endif

#include "co/co.h"
#include "co/mem.h"
#include "co/flag.h"
#include "co/log.h"
#include "co/stl.h"
#include "co/time.h"
#include "co/closure.h"
#include "co/fastream.h"
#include "context/context.h"

#if defined(_WIN32)
#include "epoll/iocp.h"
#elif defined(__linux__)
#include "epoll/epoll.h"
#else
#include "epoll/kqueue.h"
#endif

DEC_uint32(co_sched_num);
DEC_uint32(co_stack_size);
DEC_bool(co_sched_log);

#define SCHEDLOG DLOG_IF(FLG_co_sched_log)

namespace co {
namespace xx {

class Sched;
struct Coroutine;
typedef co::multimap<int64, Coroutine*>::iterator timer_id_t;

enum state_t : uint8 {
    st_wait = 0,    // wait for an event, do not modify
    st_ready = 1,   // ready to resume
    st_timeout = 2, // timeout
};

// waiting context
struct waitx_t {
    Coroutine* co;
    union { uint8 state; void* dummy; };
};

// @n: bytes to allocate, sizeof(waitx_t) by default
inline waitx_t* make_waitx(void* co, size_t n=sizeof(waitx_t)) {
    waitx_t* w = (waitx_t*) co::alloc(n); assert(w);
    w->co = (Coroutine*)co;
    w->state = st_wait;
    return w;
}

struct Coroutine {
    Coroutine() { memset(this, 0, sizeof(*this)); }
    ~Coroutine() { it.~timer_id_t(); stack.~fastream(); }

    uint32 id;        // coroutine id
    uint32 sid;       // stack id
    waitx_t* waitx;   // waiting context
    tb_context_t ctx; // coroutine context, points to the stack bottom

    // for saving stack data of this coroutine
    union { fastream stack; char _dummy1[sizeof(fastream)]; };
    union { timer_id_t it;  char _dummy2[sizeof(timer_id_t)]; };

    // Once the coroutine starts, we no longer need the cb, and it can
    // be used to store the scheduler pointer.
    union {
        Closure* cb; // coroutine function
        Sched* s;    // scheduler this coroutine runs in
    };
};

// pool of Coroutine, using index as the coroutine id.
class Copool {
  public:
    // _tb(14, 14) can hold 2^28=256M coroutines.
    Copool()
        : _tb(14, 14), _ids(1u << 14), _id(0) {
    }

    ~Copool() {
        for (int i = 0; i < _id; ++i) _tb[i].~Coroutine();
    }

    Coroutine* pop() {
        if (!_ids.empty()) {
            auto& co = _tb[_ids.pop_back()];
            co.ctx = 0;
            co.stack.clear();
            return &co;
        } else {
            auto& co = _tb[_id];
            co.id = _id++;
            co.sid = (co.id & 7);
            return &co;
        }
    }

    void push(Coroutine* co) {
        _ids.push_back(co->id);
        if (_ids.size() >= 1024) co->stack.reset();
    }

    Coroutine* operator[](size_t i) {
        return &_tb[i];
    }

  private:
    co::table<Coroutine> _tb;
    co::array<int> _ids; // id of available coroutines in the table
    int _id;
};

// Tasks may be added from any thread. We need a Mutex here.
class TaskManager {
  public:
    TaskManager() = default;
    ~TaskManager() = default;

    void add_new_task(Closure* cb) {
        std::lock_guard<std::mutex> g(_mtx);
        _new_tasks.push_back(cb);
    }

    void add_ready_task(Coroutine* co) {
        std::lock_guard<std::mutex> g(_mtx);
        _ready_tasks.push_back(co);
    }

    void get_all_tasks(
        co::array<Closure*>& new_tasks,
        co::array<Coroutine*>& ready_tasks
    ) {
        std::lock_guard<std::mutex> g(_mtx);
        if (!_new_tasks.empty()) _new_tasks.swap(new_tasks);
        if (!_ready_tasks.empty()) _ready_tasks.swap(ready_tasks);
    }
 
  private:
    std::mutex _mtx;
    co::array<Closure*> _new_tasks;
    co::array<Coroutine*> _ready_tasks;
};

inline fastream& operator<<(fastream& fs, const timer_id_t& id) {
    return fs << *(void**)(&id);
}

// Timer must be added in the scheduler thread. We need no lock here.
class TimerManager {
  public:
    TimerManager() : _timer(), _it(_timer.end()) {}
    ~TimerManager() = default;

    timer_id_t add_timer(uint32 ms, Coroutine* co) {
        return _it = _timer.insert(_it, std::make_pair(now::ms() + ms, co));
    }

    void del_timer(const timer_id_t& it) {
        if (_it == it) ++_it;
        _timer.erase(it);
    }

    timer_id_t end() {
        return _timer.end();
    }

    // return time(ms) to wait for the next timeout.
    // all timedout coroutines will be pushed into @res.
    uint32 check_timeout(co::array<Coroutine*>& res);

  private:
    co::multimap<int64, Coroutine*> _timer;        // timed-wait tasks: <time_ms, co>
    co::multimap<int64, Coroutine*>::iterator _it; // make insert faster with this hint
};

struct Stack {
    char* p;       // stack pointer 
    char* top;     // stack top
    Coroutine* co; // coroutine owns this stack
};

// coroutine scheduler 
//   - Each scheduler will loop in a single thread.
class Sched {
  public:
    Sched(uint32 id, uint32 sched_num, uint32 stack_size);
    ~Sched();

    // id of this scheduler
    uint32 id() const { return _id; }

    // the current running coroutine
    Coroutine* running() const { return _running; }

    // id of the current running coroutine
    int coroutine_id() const {
        return _sched_num * (_running->id - 1) + _id;
    }

    // check whether a pointer is on the stack of the coroutine
    bool on_stack(const void* p) const {
        Stack* const s =  &_stack[_running->sid];
        return (s->p <= (char*)p) && ((char*)p < s->top);
    }

    // resume a coroutine
    void resume(Coroutine* co);

    // suspend the current coroutine
    void yield() {
        if (_running->s != this) _running->s = this;
        tb_context_jump(_main_co->ctx, _running);
    }

    // add a new task to run as a coroutine later (thread-safe)
    void add_new_task(Closure* cb) {
        _task_mgr.add_new_task(cb);
        _epoll->signal();
    }

    // add a coroutine ready to resume (thread-safe)
    void add_ready_task(Coroutine* co) {
        _task_mgr.add_ready_task(co);
        _epoll->signal();
    }

    // sleep for milliseconds in the current coroutine 
    void sleep(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        (void) _timer_mgr.add_timer(ms, _running);
        this->yield();
    }

    // Add a timer for the current coroutine. Users should call yield() to suspend 
    // the coroutine. When the timer expires, the scheduler will resume it again.
    void add_timer(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        _running->it = _timer_mgr.add_timer(ms, _running);
        SCHEDLOG << "co(" << _running << ") add timer " << _running->it << " (" << ms << " ms)" ;
    }

    // check whether the current coroutine has timed out
    bool timeout() const { return _timeout; }

    // add an IO event on a socket to epoll for the current coroutine.
    bool add_io_event(sock_t fd, io_event_t ev) {
        SCHEDLOG << "co(" << _running << ") add io event fd: " << fd << " ev: " << (int)ev;
      #if defined(_WIN32)
        (void) ev; // we do not care what the event is on windows
        return _epoll->add_event(fd);
      #elif defined(__linux__)
        if (ev == ev_read) return _epoll->add_ev_read(fd, _running->id);
        return _epoll->add_ev_write(fd, _running->id);
      #else
        if (ev == ev_read) return _epoll->add_ev_read(fd, _running);
        return _epoll->add_ev_write(fd, _running);
      #endif
    }

    // delete an IO event on a socket from the epoll for the current coroutine.
    void del_io_event(sock_t fd, io_event_t ev) {
        SCHEDLOG << "co(" << _running << ") del io event, fd: " << fd << " ev: " << (int)ev;
        ev == ev_read ? _epoll->del_ev_read(fd) : _epoll->del_ev_write(fd);
    }

    // delete all IO events on a socket from the epoll.
    void del_io_event(sock_t fd) {
        SCHEDLOG << "co(" << _running << ") del io event, fd: " << fd;
        _epoll->del_event(fd);
    }

    int64 cputime() {
        return atomic_load(&_cputime, mo_relaxed);
    }

    // start the scheduler thread
    void start() { std::thread(&Sched::loop, this).detach(); }

    // stop the scheduler thread
    void stop();

    // the thread function
    void loop();

  private:
    friend class SchedManager;

    // Entry function for coroutines
    static void main_func(tb_context_from_t from);

    // push a coroutine back to the pool, so it can be reused later.
    void recycle() {
        _stack[_running->sid].co = 0;
        _co_pool.push(_running);
    }

    // save stack for the coroutine
    void save_stack(Coroutine* co) {
        if (co) {
            co->stack.clear();
            co->stack.append(co->ctx, _stack[co->sid].top - (char*)co->ctx);
        }
    }

    // pop a Coroutine from the pool
    Coroutine* new_coroutine(Closure* cb) {
        Coroutine* co = _co_pool.pop();
        co->cb = cb;
        co->it = _timer_mgr.end();
        return co;
    }

  private:
    Epoll* _epoll;
    uint32 _wait_ms;     // time in milliseconds the epoll to wait for
    uint32 _id;          // scheduler id
    uint32 _sched_num;   // scheduler num
    uint32 _stack_size;  // size of stack
    Stack* _stack;       // pointer to stack list
    Coroutine* _main_co; // save the main context
    Coroutine* _running; // the current running coroutine

    Copool _co_pool;
    TaskManager _task_mgr;
    TimerManager _timer_mgr;

    int64 _cputime;
    co::sync_event _ev;
    bool _stop;
    bool _timeout;
};

class SchedManager {
  public:
    SchedManager();
    ~SchedManager();

    Sched* next_sched() const { return _next(_scheds); }

    const co::vector<Sched*>& scheds() const {
        return _scheds;
    }

    void stop();

  private:
    std::function<Sched*(const co::vector<Sched*>&)> _next;
    co::vector<Sched*> _scheds;
};

inline bool& is_active() {
    static bool ka = false;
    return ka;
}

extern __thread Sched* gSched;

} // xx
} // co
