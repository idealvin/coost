#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4127)
#endif

#include "co/co.h"
#include "co/god.h"
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
DEC_uint32(co_stack_num);
DEC_uint32(co_stack_size);
DEC_bool(co_sched_log);

#define SCHEDLOG DLOG_IF(FLG_co_sched_log)

namespace co {
namespace xx {

struct SchedInitializer {
    SchedInitializer();
    ~SchedInitializer();
};

static SchedInitializer g_sched_initializer;

class Sched;
struct Coroutine;
typedef co::multimap<int64, Coroutine*>::iterator timer_id_t;

enum state_t : uint8 {
    st_wait = 0,    // wait for an event, do not modify
    st_ready = 1,   // ready to resume
    st_timeout = 2, // timeout
};

// waiting context
struct waitx_t : co::clink {
    Coroutine* co;
    union { uint8 state; void* dummy; };
};

inline waitx_t* make_waitx(void* co, size_t n=sizeof(waitx_t)) {
    waitx_t* w = (waitx_t*) co::alloc(n); assert(w);
    w->next = w->prev = 0;
    w->co = (Coroutine*)co;
    w->state = st_wait;
    return w;
}

struct Stack {
    char* p;       // stack pointer 
    char* top;     // stack top
    Coroutine* co; // coroutine owns this stack
};

struct Buffer {
    struct H {
        uint32 cap;
        uint32 size;
        char p[];
    };

    Buffer() = delete;
    ~Buffer() = delete;

    const char* data() const noexcept { return _h ? _h->p : 0; }
    uint32 size() const noexcept { return _h ? _h->size : 0; }
    uint32 capacity() const noexcept { return _h ? _h->cap : 0; }
    void clear() noexcept { if (_h) _h->size = 0; }

    void reset() {
        if (_h) {
            co::free(_h, _h->cap + 8);
            _h = 0;
        }
    }

    void append(const void* p, size_t size) {
        const uint32 n = (uint32)size;
        if (!_h) {
            _h = (H*) co::alloc(size + 8); assert(_h);
            _h->cap = n;
            _h->size = 0;
            goto lable;
        }

        if (_h->cap < _h->size + n) {
            const uint32 o = _h->cap;
            _h->cap += (o >> 1) + n;
            _h = (H*) co::realloc(_h, o + 8, _h->cap + 8); assert(_h);
            goto lable;
        }

      lable:
        memcpy(_h->p + _h->size, p, n);
        _h->size += n;
    }

    H* _h;
};

struct Coroutine {
    Coroutine() = delete;
    ~Coroutine() = delete;

    uint32 id;        // coroutine id
    tb_context_t ctx; // coroutine context, points to the stack bottom
    Closure* cb;      // coroutine function
    Sched* sched;     // scheduler this coroutine runs in
    Stack* stack;     // stack this coroutine runs on
    union {
        Buffer buf;   // for saving stack data of this coroutine
        void* pbuf;
    };
    waitx_t* waitx;   // waiting context
    union { timer_id_t it;  char _dummy2[sizeof(timer_id_t)]; };
};

class CoroutinePool {
  public:
    static const int E = 12;
    static const int N = 1 << E; // max coroutines per block
    static const int M = 256;

    CoroutinePool()
        : _c(0), _o(0), _v(M), _use_count(M) {
        _v.resize(M);
        _use_count.resize(M);
    }

    ~CoroutinePool() {
        for (size_t i = 0; i < _v.size(); ++i) {
            if (_v[i]) ::free(_v[i]);
        }
        _v.clear();
    }

    Coroutine* pop() {
        int id = 0;
        if (!_v0.empty()) { id = _v0.pop_back(); goto reuse; }
        if (!_vc.empty()) { id = _vc.pop_back(); goto reuse; }
        if (_o < N) goto newco;
        _c = !_blks.empty() ? *_blks.begin() : _c + 1;
        if (!_blks.empty()) _blks.erase(_blks.begin());
        _o = 0;

      newco:
        {
            if (_c < _v.size()) {
                if (!_v[_c]) _v[_c] = (Coroutine*) ::calloc(N, sizeof(Coroutine));
            } else {
                const int c = god::align_up<M>(_c + 1);
                _v.resize(c);
                _use_count.resize(c);
                _v[_c] = (Coroutine*) ::calloc(N, sizeof(Coroutine));
            }

            auto& co = _v[_c][_o];
            co.id = (_c << E) + _o++;
            _use_count[_c]++;
            return &co;
        }

      reuse:
        {
            const int q = id >> E;
            const int r = id & (N - 1);
            auto& co = _v[q][r];
            co.ctx = 0;
            _use_count[q]++;
            return &co;
        }
    }

    void push(Coroutine* co) {
        const int id = co->id;
        const int q = id >> E;
        if (q == 0) {
            if (_v0.capacity() == 0) _v0.reserve(N);
            _v0.push_back(id);
            goto end;
        }
        if (q == _c) {
            if (_vc.capacity() == 0) _vc.reserve(N);
            _vc.push_back(id);
            goto end;
        }

      end:
        if (--_use_count[q] == 0) {
            ::free(_v[q]);
            _v[q] = 0;
            if (q != _c) {
                _blks.insert(q);
            } else {
                _vc.clear();
                _o = 0;
                if (!_blks.empty() && *_blks.begin() < _c) {
                    _blks.insert(_c);
                    _c = *_blks.begin();
                    _blks.erase(_blks.begin());
                }
            }
        }
    }

    Coroutine& operator[](int i) const {
        const int q = i >> E;
        const int r = i & (N - 1);
        return _v[q][r];
    }

  private:
    int _c; // current block
    int _o; // offset in the current block [0, S)
    co::vector<Coroutine*> _v;
    co::vector<int> _use_count;
    co::vector<int> _v0; // id of coroutine in _v[0]
    co::vector<int> _vc; // id of coroutine in _v[_c]
    co::set<int> _blks; // blocks available
};

// Task may be added from any thread. We need a mutex here.
class alignas(co::cache_line_size) TaskManager {
  public:
    TaskManager() : _mtx(), _new_tasks(512), _ready_tasks(512) {}
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
        co::vector<Closure*>& new_tasks,
        co::vector<Coroutine*>& ready_tasks
    ) {
        std::lock_guard<std::mutex> g(_mtx);
        if (!_new_tasks.empty()) _new_tasks.swap(new_tasks);
        if (!_ready_tasks.empty()) _ready_tasks.swap(ready_tasks);
    }
 
  private:
    std::mutex _mtx;
    co::vector<Closure*> _new_tasks;
    co::vector<Coroutine*> _ready_tasks;
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
        return _it = _timer.emplace_hint(_it, now::ms() + ms, co);
    }

    void del_timer(const timer_id_t& it) {
        if (_it == it) ++_it;
        _timer.erase(it);
    }

    timer_id_t end() { return _timer.end(); }

    // get timedout coroutines, return time(ms) to wait for the next timeout
    uint32 check_timeout(co::vector<Coroutine*>& res);

  private:
    co::multimap<int64, Coroutine*> _timer;        // timed-wait tasks: <time_ms, co>
    co::multimap<int64, Coroutine*>::iterator _it; // make insert faster with this hint
};

// coroutine scheduler, loop in a single thread
class Sched {
  public:
    Sched(uint32 id, uint32 sched_num, uint32 stack_num, uint32 stack_size);
    ~Sched();

    // id of this scheduler
    uint32 id() const { return _id; }

    // the current running coroutine
    Coroutine* running() const { return _running; }

    // id of the current running coroutine
    int coroutine_id() const {
        return _sched_num * (_running->id - 1) + _id;
    }

    // check if the memory @p points to is on the stack of the coroutine
    bool on_stack(const void* p) const {
        Stack* const s =  _running->stack;
        return (s->p <= (char*)p) && ((char*)p < s->top);
    }

    // resume a coroutine
    void resume(Coroutine* co);

    // suspend the current coroutine
    void yield() {
        tb_context_jump(_main_co->ctx, _running);
    }

    // add a new task to run as a coroutine later (thread-safe)
    void add_new_task(Closure* cb) {
        _task_mgr.add_new_task(cb);
        _x.epoll->signal();
    }

    // add a coroutine ready to resume (thread-safe)
    void add_ready_task(Coroutine* co) {
        _task_mgr.add_ready_task(co);
        _x.epoll->signal();
    }

    // sleep for milliseconds in the current coroutine 
    void sleep(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        (void) _timer_mgr.add_timer(ms, _running);
        this->yield();
    }

    // add a timer for the current coroutine
    void add_timer(uint32 ms) {
        if (_wait_ms > ms) _wait_ms = ms;
        _running->it = _timer_mgr.add_timer(ms, _running);
        SCHEDLOG << "co(" << _running << ") add timer " << _running->it << " (" << ms << " ms)" ;
    }

    // check whether the current coroutine has timed out
    bool timeout() const { return _timeout; }

    // add an IO event on a socket to epoll for the current coroutine.
    bool add_io_event(sock_t fd, _ev_t ev) {
        SCHEDLOG << "co(" << _running << ") add io event fd: " << fd << " ev: " << (int)ev;
      #if defined(_WIN32)
        (void) ev; // we do not care what the event is on windows
        return _x.epoll->add_event(fd);
      #elif defined(__linux__)
        return ev == ev_read ? _x.epoll->add_ev_read(fd, _running->id) : _x.epoll->add_ev_write(fd, _running->id);
      #else
        return ev == ev_read ? _x.epoll->add_ev_read(fd, _running) : _x.epoll->add_ev_write(fd, _running);
      #endif
    }

    // delete an IO event on a socket from the epoll for the current coroutine.
    void del_io_event(sock_t fd, _ev_t ev) {
        SCHEDLOG << "co(" << _running << ") del io event, fd: " << fd << " ev: " << (int)ev;
        ev == ev_read ? _x.epoll->del_ev_read(fd) : _x.epoll->del_ev_write(fd);
    }

    // delete all IO events on a socket from the epoll.
    void del_io_event(sock_t fd) {
        SCHEDLOG << "co(" << _running << ") del io event, fd: " << fd;
        _x.epoll->del_event(fd);
    }

    // cputime of this scheduler (us)
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
    // entry function for coroutine
    static void main_func(tb_context_from_t from);

    // save stack for the coroutine
    void save_stack(Coroutine* co) {
        if (co) {
            if (!co->pbuf && !_bufs.empty()) co->pbuf = _bufs.pop_back();
            co->buf.clear();
            co->buf.append(co->ctx, co->stack->top - (char*)co->ctx);
        }
    }

    // pop a Coroutine from the pool
    Coroutine* new_coroutine(Closure* cb) {
        Coroutine* co = _co_pool.pop();
        co->cb = cb;
        if (!co->sched) {
            co->sched = this;
            co->stack = &_stack[co->id & (_stack_num - 1)];
        }
        new(&co->it) timer_id_t(_timer_mgr.end());
        return co;
    }

    void recycle(Coroutine* co) {
        co->it.~timer_id_t();
        if (co->pbuf) {
            if (co->buf.capacity() > 8192 || _bufs.size() >= 128) {
                co->buf.reset();
                goto end;
            }
            _bufs.push_back(co->pbuf);
            co->pbuf = 0;
        }

      end:
        _co_pool.push(co);
    }

  private:
    union {
        int64 _cputime;
        char _c0[co::cache_line_size];
    };
    union {
        struct {
            co::sync_event ev;
            Epoll* epoll;
            bool stopped;
        }_x;
        char _c1[co::cache_line_size];
    };
    TaskManager _task_mgr;

    TimerManager _timer_mgr;
    uint32 _wait_ms;     // time the epoll to wait for
    bool _timeout;
    co::vector<void*> _bufs;
    CoroutinePool _co_pool;
    Coroutine* _running; // the current running coroutine
    Coroutine* _main_co; // save the main context
    uint32 _id;          // scheduler id
    uint32 _sched_num;   // number of schedulers 
    uint32 _stack_num;   // number of stacks per scheduler
    uint32 _stack_size;  // size of the stack
    Stack* _stack;       // stack array
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

static bool g_is_active;
inline bool& is_active() { return g_is_active; }

extern __thread Sched* gSched;

} // xx
} // co
