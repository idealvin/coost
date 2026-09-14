#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4127)
#endif

#include "buffer.h"
#include "idgen.h"
#include "co/clist.h"
#include "co/closure.h"
#include "co/mem.h"
#include "co/stl.h"
#include "co/rand.h"
#include "co/time.h"
#include "co/thread.h"
#include "context/context.h"

#if defined(_WIN32)
#include "epoll/iocp.h"
#elif defined(__linux__)
#include "epoll/epoll.h"
#else
#include "epoll/kqueue.h"
#endif

#ifdef CO_DEBUG_COROUTINE
#include "co/log.h"
#define CLOG log::debug
#else
#define CLOG(...)
#endif // ifdef CO_DEBUG_COROUTINE


namespace co {

extern uint32 g_sched_num;
extern uint32 g_stack_num;
extern uint32 g_stack_size;

struct Coroutine;
struct Sched;

extern __thread Sched* g_sched;
extern co::vector<Sched*>* g_all_scheds;

typedef co::multimap<int64, Coroutine*>::iterator timer_id_t;

inline co::string& operator<<(co::string& s, const timer_id_t& id) {
    return s << *(void**)(&id);
}

// waiting context, for co::event
struct __cacheline_aligned Waitx : co::clink {
    Coroutine* co;
    union {
        uint8 state;
        void* ud;
    };
};

// offsetof(Coroutine, wtx) == offsetof(Waitx, co)
struct Coroutine {
    Coroutine() = delete;
    ~Coroutine() = delete;

    union {
        co::clink lnk;
        once_closure c;
        void* pc;
    };

    Waitx* wtx;       // waiting context, used to implement co::mutex, co::event
    tb_context_t ctx; // coroutine context, points to the stack bottom
    Sched* sched;     // scheduler this coroutine runs in
    int32 id;         // coroutine id
    bool has_timer;
    union { timer_id_t it;  char _dummy2[sizeof(timer_id_t)]; };
    union {
        Buffer buf;   // for saving stack data of this coroutine
        void* pbuf;
    };
};

struct __cacheline_aligned TaskManager {
    TaskManager() : _mtx() {
        _new_tasks.reserve(512);
        _ready_tasks.reserve(512);
    }
    ~TaskManager() = default;

    void add_new_task(void* c) {
        std::lock_guard<std::mutex> g(_mtx);
        _new_tasks.push_back(c);
    }

    void add_ready_task(Coroutine* co) {
        std::lock_guard<std::mutex> g(_mtx);
        _ready_tasks.push_back(co);
    }

    void get_all_tasks(
        co::vector<void*>& new_tasks,
        co::vector<Coroutine*>& ready_tasks
    ) {
        std::lock_guard<std::mutex> g(_mtx);
        if (!_new_tasks.empty()) _new_tasks.swap(new_tasks);
        if (!_ready_tasks.empty()) _ready_tasks.swap(ready_tasks);
    }

    size_t steal_tasks(co::vector<void*>& v) {
        std::lock_guard<std::mutex> g(_mtx);
        if (!_new_tasks.empty()) {
            const size_t r = _new_tasks.size() >> 1;
            const size_t n = _new_tasks.size() - r; // stealed
            v.reserve(n);
            ::memcpy(v.data(), &_new_tasks[r], n * sizeof(void*));
            _new_tasks.resize(r);
            return n;
        }
        return 0;
    }

    std::mutex _mtx;
    co::vector<void*> _new_tasks;
    co::vector<Coroutine*> _ready_tasks;
};

struct TimerManager {
    TimerManager() : _timer(), _it(_timer.end()) {}
    ~TimerManager() = default;

    timer_id_t add_timer(uint32 ms, Coroutine* co) {
        return _it = _timer.emplace_hint(_it, time::mono.ms() + ms, co);
    }

    void del_timer(const timer_id_t& it) {
        if (_it == it) ++_it;
        _timer.erase(it);
    }

    // get timedout coroutines, return time(ms) to wait for the next timeout
    uint32 check_timeout(co::vector<Coroutine*>& res);

    co::multimap<int64, Coroutine*> _timer;        // timed-wait tasks: <time_ms, co>
    co::multimap<int64, Coroutine*>::iterator _it; // make insert faster with this hint
};

struct Stack {
    char* p;       // stack pointer 
    char* top;     // stack top
    Coroutine* co; // coroutine owns this stack
};

// coroutine scheduler, loop in a single thread
struct Sched {
    Sched(uint32 id);
    ~Sched();

    #define S 's',_id
    #define SC 's',_id,'c',_running->id,'(',_running,')'

    uint32 id() const { return _id; }

    // the current running coroutine
    Coroutine* running() const { return _running; }

    // current coroutine id
    int coroutine_id() const {
        return g_sched_num * (_running->id - 1) + _id;
    }

    Stack* get_stack(uint32 co_id) const {
        return &_stack[co_id & (g_stack_num - 1)];
    }

    bool on_stack(const void* p) const {
        Stack* const s =  this->get_stack(_running->id);
        return (s->p <= (char*)p) && ((char*)p < s->top);
    }

    void resume(Coroutine* co);

    // suspend the current coroutine
    void yield() {
        CLOG(SC, " yield -> main ctx: ", _main_co->ctx);
        const tb_context_from_t from = tb_context_jump(_main_co->ctx, _running);
        CLOG(SC, " yield back from main ctx: ", from.ctx);
        ((Coroutine*)from.priv)->ctx = from.ctx;
    }

    void add_new_task(void* c) {
        _task_mgr.add_new_task(c);
        _epoll->signal();
    }

    void add_ready_task(Coroutine* co) {
        _task_mgr.add_ready_task(co);
        _epoll->signal();
    }

    // steal tasks from other scheduler
    size_t steal_tasks() {
        const uint32 r = co::rand(_seed) % (g_sched_num - 1);
        const uint32 k = r < _id ? r : r + 1;
        const size_t n = (*g_all_scheds)[k]->_task_mgr.steal_tasks(_new_tasks);
        CLOG(S, " steal ", n, " tasks from s", k);
        return n;
    }

    void sleep(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        (void) _timer_mgr.add_timer(ms, _running);
        this->yield();
    }

    void add_timer(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        _running->has_timer = true;
        _running->it = _timer_mgr.add_timer(ms, _running);
        CLOG(SC, " add timer ", _running->it, " (", ms, " ms)");
    }

    bool timeout() const { return _timeout; }

    bool add_ev_read(sock_t fd) {
        CLOG(SC, " add ev_read, fd: ", fd);
        return _epoll->add_ev_read(fd, _running);
    }

    bool add_ev_write(sock_t fd) {
        CLOG(SC, " add ev_write, fd: ", fd);
        return _epoll->add_ev_write(fd, _running);
    }

    void del_ev_read(sock_t fd) {
        CLOG(SC, " del ev_read, fd: ", fd);
        _epoll->del_ev_read(fd);
    }

    void del_ev_write(sock_t fd) {
        CLOG(SC, " del ev_write, fd: ", fd);
        _epoll->del_ev_write(fd);
    }

    bool add_io_event(sock_t fd, ev_t ev) {
        return ev == ev_read ? add_ev_read(fd) : add_ev_write(fd);
    }

    void del_io_event(sock_t fd, ev_t ev) {
        ev == ev_read ? del_ev_read(fd) : del_ev_write(fd);
    }

    // delete all IO events on a socket from the epoll.
    void del_io_event(sock_t fd) {
        CLOG(SC, " del io event, fd: ", fd);
        _epoll->del_event(fd);
    }

    void start() { std::thread(&Sched::loop, this).detach(); }

    void stop();

    void loop();

    // entry function for coroutine
    static void main_func(tb_context_from_t from);

    static void save_stack(Coroutine* co, Stack* s) {
        if (co) {
            co->buf.clear();
            co->buf.append(co->ctx, s->top - (char*)co->ctx);
        }
    }

    static void restore_stack(Coroutine* co, Stack* s) {
        if (co->buf.size() > 0) {
            runtime_assert(s->top == (char*)co->ctx + co->buf.size());
            ::memcpy(co->ctx, co->buf.data(), co->buf.size());
        }
    }

    Coroutine* new_coroutine(void* c) {
        Coroutine* co = (Coroutine*) co::alloc(sizeof(Coroutine));
        runtime_assert(co);
        co->pc = c;
        co->wtx = 0;
        co->ctx = 0;
        co->sched = this;
        co->id = _idgen.pop();
        co->has_timer = false;
        co->pbuf = 0;
        return co;
    }

    void free_coroutine(Coroutine* co) {
        static_assert(std::is_trivially_destructible_v<timer_id_t>, "");
        co->buf.reset();
        _idgen.push(co->id);
        co::free(co, sizeof(Coroutine));
    }

    TaskManager _task_mgr;
    TimerManager _timer_mgr;
    Epoll* _epoll;

    uint32 _id; // scheduler id
    uint32 _seed;
    co::vector<void*> _new_tasks;
    co::vector<Coroutine*> _ready_tasks;

    IdGen _idgen;
    co::sync_event _ev;
    bool _stopped;
    bool _timeout;
    uint32 _wait_ms;     // time the epoll to wait for
    Coroutine* _running; // the current running coroutine
    Coroutine* _main_co; // save the main context
    Stack* _stack;       // stack array
};

} // co
