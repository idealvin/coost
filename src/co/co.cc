#include "scheduler.h"
#include "co/stl.h"

#ifndef _WIN32
#ifdef __linux__
#include <unistd.h>       // for syscall()
#include <sys/syscall.h>  // for SYS_xxx definitions
#include <time.h>         // for clock_gettime
#else
#include <sys/time.h>     // for gettimeofday
#endif
#endif

namespace co {
namespace xx {

#ifdef _WIN32
typedef CRITICAL_SECTION mutex_t;
typedef CONDITION_VARIABLE cv_t;

inline void cv_init(cv_t* c) { InitializeConditionVariable(c); }
inline void cv_free(cv_t*) {}
inline void cv_wait(cv_t* c, mutex_t* m) { SleepConditionVariableCS(c, m, INFINITE); }
inline bool cv_wait(cv_t* c, mutex_t* m, uint32 ms) { return SleepConditionVariableCS(c, m, ms) == TRUE; }
inline void cv_notify_one(cv_t* c) { WakeConditionVariable(c); }
inline void cv_notify_all(cv_t* c) { WakeAllConditionVariable(c); }

uint32 thread_id() { return GetCurrentThreadId(); }

class mutex {
  public:
    mutex() { InitializeCriticalSection(&_m); }
    ~mutex() { DeleteCriticalSection(&_m); }

    void lock() { EnterCriticalSection(&_m); }
    void unlock() { LeaveCriticalSection(&_m); }
    bool try_lock() { return TryEnterCriticalSection(&_m) != FALSE; }
    mutex_t* native_handle() { return &_m; } 

  private:
    mutex_t _m;
    DISALLOW_COPY_AND_ASSIGN(mutex);
};

#else
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cv_t;

inline void cv_init(cv_t* c) {
  #ifdef __linux__
    pthread_condattr_t attr;
    int r = pthread_condattr_init(&attr); assert(r == 0);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    r = pthread_cond_init(c, &attr); assert(r == 0);
    pthread_condattr_destroy(&attr);
  #else
    int r = pthread_cond_init(c, 0); (void)r; assert(r == 0);
  #endif
}

bool cv_wait(cv_t* c, mutex_t* m, uint32 ms) {
  #ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
  #else
    struct timespec ts;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
  #endif

    if (ms < 1000) {
        ts.tv_nsec += ms * 1000000;
    } else {
        const uint32 x = ms / 1000;
        ts.tv_nsec += (ms - x * 1000) * 1000000;
        ts.tv_sec += x;
    }

    if (ts.tv_nsec > 999999999) {
        ts.tv_nsec -= 1000000000;
        ++ts.tv_sec;
    }

    return pthread_cond_timedwait(c, m, &ts) == 0;
}

inline void cv_free(cv_t* c) { pthread_cond_destroy(c); }
inline void cv_wait(cv_t* c, mutex_t* m) { pthread_cond_wait(c, m); }
inline void cv_notify_one(cv_t* c) { pthread_cond_signal(c); }
inline void cv_notify_all(cv_t* c) { pthread_cond_broadcast(c); }

#ifdef __linux__
#ifndef SYS_gettid
#define SYS_gettid __NR_gettid
#endif
uint32 thread_id() { return syscall(SYS_gettid); }

#else /* for mac, bsd.. */
uint32 thread_id() {
    uint64 x;
    pthread_threadid_np(0, &x);
    return (uint32)x;
}
#endif

class mutex {
  public:
    mutex() { const int r = pthread_mutex_init(&_m, 0); (void)r; assert(r == 0); }
    ~mutex() { const int r = pthread_mutex_destroy(&_m); (void)r; assert(r == 0); }

    void lock() { const int r = pthread_mutex_lock(&_m); (void)r; assert(r == 0); }
    void unlock() { const int r = pthread_mutex_unlock(&_m); (void)r; assert(r == 0); }
    bool try_lock() { return pthread_mutex_trylock(&_m) == 0; }
    mutex_t* native_handle() { return &_m; }

  private:
    mutex_t _m;
    DISALLOW_COPY_AND_ASSIGN(mutex);
};
#endif

class mutex_guard {
  public:
    explicit mutex_guard(mutex& lock) : _lock(lock) { _lock.lock(); }
    ~mutex_guard() { _lock.unlock(); }

  private:
    mutex& _lock;
    DISALLOW_COPY_AND_ASSIGN(mutex_guard);
};

} // xx

class mutex_impl {
  public:
    mutex_impl() : _lock(0), _has_cv(false) {}
    ~mutex_impl() { if (_has_cv) xx::cv_free(&_cv); }

    void lock();
    void unlock();
    bool try_lock();

  private:
    xx::mutex _m;
    xx::cv_t _cv;
    co::deque<Coroutine*> _co_wait;
    int32 _lock;
    bool _has_cv;
};

inline bool mutex_impl::try_lock() {
    xx::mutex_guard g(_m);
    return _lock ? false : (_lock = 1);
}

void mutex_impl::lock() {
    auto s = gSched;
    if (s) { /* in coroutine */
        _m.lock();
        if (!_lock) {
            _lock = 1;
            _m.unlock();
        } else {
            Coroutine* co = s->running();
            if (co->s != s) co->s = s;
            _co_wait.push_back(co);
            _m.unlock();
            s->yield();
        }

    } else { /* non-coroutine */
        xx::mutex_guard g(_m);
        if (!_lock) {
            _lock = 1;
        } else {
            _co_wait.push_back(nullptr);
            if (!_has_cv) { xx::cv_init(&_cv); _has_cv = true; }
            for (;;) {
                xx::cv_wait(&_cv, _m.native_handle());
                if (_lock == 2) { _lock = 1; break; }
            }
        }
    }
}

void mutex_impl::unlock() {
    _m.lock();
    if (_co_wait.empty()) {
        _lock = 0;
        _m.unlock();
    } else {
        Coroutine* co = _co_wait.front();
        _co_wait.pop_front();
        if (co) {
            _m.unlock();
            ((SchedulerImpl*)co->s)->add_ready_task(co);
        } else {
            _lock = 2;
            _m.unlock();
            xx::cv_notify_one(&_cv);
        }
    }
}

// memory: |4(refn)|4|mutex_impl|
mutex::mutex() {
    _p = (uint32*) co::alloc(sizeof(mutex_impl) + 8); assert(_p);
    _p[0] = 1; // refn
    new (_p + 2) mutex_impl();
}

mutex::~mutex() {
    if (_p && atomic_dec(_p, mo_acq_rel) == 0) {
        ((mutex_impl*)(_p + 2))->~mutex_impl();
        co::free(_p, sizeof(mutex_impl) + 8);
        _p = 0;
    }
}

void mutex::lock() const {
    ((mutex_impl*)(_p + 2))->lock();
}

void mutex::unlock() const {
    ((mutex_impl*)(_p + 2))->unlock();
}

bool mutex::try_lock() const {
    return ((mutex_impl*)(_p + 2))->try_lock();
}

class event_impl {
  public:
    event_impl(bool manual_reset, bool signaled)
        : _count(0), _manual_reset(manual_reset), _signaled(signaled), _has_cv(false) {
    }
    ~event_impl() { if (_has_cv) xx::cv_free(&_cv); }

    bool wait(uint32 ms);

    void signal();

    void reset();

  private:
    xx::mutex _m;
    xx::cv_t _cv;
    co::hash_set<co::waitx_t*> _co_wait;
    union {
        uint64 _count;
        struct {
            uint32 nco; // waiting coroutines
            uint32 nth; // waiting threads
        } _wait;
    };
    const bool _manual_reset;
    bool _signaled;
    bool _has_cv;
};

bool event_impl::wait(uint32 ms) {
    auto s = gSched;
    if (s) { /* in coroutine */
        Coroutine* co = s->running();
        if (co->s != s) co->s = s;
        {
            xx::mutex_guard g(_m);
            if (_signaled) {
                if (!_manual_reset && _count == 0) _signaled = false;
                return true;
            }
            ++_wait.nco;
            co->waitx = co::make_waitx(co);
            _co_wait.insert(co->waitx);
        }

        if (ms != (uint32)-1) s->add_timer(ms);
        s->yield();
        if (!s->timeout()) {
            {
                xx::mutex_guard g(_m);
                --_wait.nco;
                if (!_manual_reset && _count == 0) _signaled = false;
            }
            co::free(co->waitx, sizeof(co::waitx_t));
        } else {
            bool erased;
            {
                xx::mutex_guard g(_m);
                --_wait.nco;
                if (_signaled && !_manual_reset && _count == 0) _signaled = false;
                erased = _co_wait.erase(co->waitx) == 1;
            }
            if (erased) co::free(co->waitx, sizeof(co::waitx_t));
        }

        co->waitx = nullptr;
        return !s->timeout();

    } else { /* not in coroutine */
        xx::mutex_guard g(_m);
        if (!_signaled) {
            ++_wait.nth;
            if (!_has_cv) { xx::cv_init(&_cv); _has_cv = true; }
            bool r = true;
            if (ms != (uint32)-1) {
                r = xx::cv_wait(&_cv, _m.native_handle(), ms);
            } else {
                xx::cv_wait(&_cv, _m.native_handle());
            }
            --_wait.nth;
            if (!r) return false;
            assert(_signaled);
        }
        if (!_manual_reset && _count == 0) _signaled = false;
        return true;
    }
}

void event_impl::signal() {
    co::hash_set<co::waitx_t*> co_wait;
    {
        xx::mutex_guard g(_m);
        if (!_co_wait.empty()) _co_wait.swap(co_wait);
        if (!_signaled) {
            _signaled = true;
            if (_wait.nth > 0) xx::cv_notify_all(&_cv);
        }
    }

    // Using atomic operation here, as check_timeout() in the Scheduler 
    // may also modify the state.
    for (auto it = co_wait.begin(); it != co_wait.end(); ++it) {
        co::waitx_t* w = *it;
        // TODO: is mo_relaxed safe here?
        if (atomic_bool_cas(&w->state, st_wait, st_ready, mo_relaxed, mo_relaxed)) {
            ((SchedulerImpl*)(w->co->s))->add_ready_task(w->co);
        } else { /* timeout */
            co::free(w, sizeof(*w));
        }
    }
}

inline void event_impl::reset() {
    xx::mutex_guard g(_m);
    _signaled = false;
}

// memory: |4(refn)|4|event_impl|
event::event(bool manual_reset, bool signaled) {
    _p = (uint32*) co::alloc(sizeof(event_impl) + 8); assert(_p);
    _p[0] = 1;
    new (_p + 2) event_impl(manual_reset, signaled);
}

event::~event() {
    if (_p && atomic_dec(_p, mo_acq_rel) == 0) {
        ((event_impl*)(_p + 2))->~event_impl();
        co::free(_p, sizeof(event_impl) + 8);
        _p = 0;
    }
}

bool event::wait(uint32 ms) const {
    return ((event_impl*)(_p + 2))->wait(ms);
}

void event::signal() const {
    ((event_impl*)(_p + 2))->signal();
}

void event::reset() const {
    ((event_impl*)(_p + 2))->reset();
}

class sync_event_impl {
  public:
    explicit sync_event_impl(bool manual_reset, bool signaled)
        : _counter(0), _manual_reset(manual_reset), _signaled(signaled) {
        xx::cv_init(&_cv);
    }

    ~sync_event_impl() {
        xx::cv_free(&_cv);
    }

    void signal() {
        xx::mutex_guard g(_m);
        if (!_signaled) {
            _signaled = true;
            xx::cv_notify_all(&_cv);
        }
    }

    void reset() {
        xx::mutex_guard g(_m);
        _signaled = false;
    }

    void wait() {
        xx::mutex_guard g(_m);
        if (!_signaled) {
            ++_counter;
            xx::cv_wait(&_cv, _m.native_handle());
            --_counter;
            assert(_signaled);
        }
        if (!_manual_reset && _counter == 0) _signaled = false;
    }

    // return false if timeout
    bool wait(uint32 ms) {
        xx::mutex_guard g(_m);
        if (!_signaled) {
            ++_counter;
            bool r = co::xx::cv_wait(&_cv, _m.native_handle(), ms);
            --_counter;
            if (!r) return false;
            assert(_signaled);
        }
        if (!_manual_reset && _counter == 0) _signaled = false;
        return true;
    }

  private:
    xx::cv_t _cv;
    xx::mutex _m;
    int _counter;
    const bool _manual_reset;
    bool _signaled;
};

sync_event::sync_event(bool manual_reset, bool signaled) {
    _p = co::alloc(sizeof(sync_event_impl)); assert(_p);
    new (_p) sync_event_impl(manual_reset, signaled);
}

sync_event::~sync_event() {
    if (_p) {
        ((sync_event_impl*)_p)->~sync_event_impl();
        co::free(_p, sizeof(sync_event_impl));
        _p = 0;
    }
}

void sync_event::signal() {
    ((sync_event_impl*)_p)->signal();
}

void sync_event::reset() {
    ((sync_event_impl*)_p)->reset();
}

void sync_event::wait() {
    ((sync_event_impl*)_p)->wait();
}

bool sync_event::wait(uint32 ms) {
    return ((sync_event_impl*)_p)->wait(ms);
}

// memory: |4(refn)|4(counter)|event_impl|
wait_group::wait_group(uint32 n) {
    _p = (uint32*) co::alloc(sizeof(event_impl) + 8); assert(_p);
    _p[0] = 1; // refn
    _p[1] = n; // counter
    new (_p + 2) event_impl(false, false);
}

wait_group::~wait_group() {
    if (_p && atomic_dec(_p, mo_acq_rel) == 0) {
        ((event_impl*)(_p + 2))->~event_impl();
        co::free(_p, sizeof(event_impl) + 8);
        _p = 0;
    }
}

void wait_group::add(uint32 n) const {
    atomic_add(_p + 1, n, mo_relaxed);
}

void wait_group::done() const {
    const uint32 x = atomic_dec(_p + 1, mo_acq_rel);
    CHECK(x != (uint32)-1);
    if (x == 0) ((event_impl*)(_p + 2))->signal();
}

void wait_group::wait() const {
    ((event_impl*)(_p + 2))->wait((uint32)-1);
}

namespace xx {

__thread bool g_done = false;

class pipe_impl {
  public:
    pipe_impl(uint32 buf_size, uint32 blk_size, uint32 ms, pipe::cpmv_t&& c, pipe::destruct_t&& d)
        : _c(std::move(c)), _d(std::move(d)), _buf_size(buf_size), _blk_size(blk_size), 
          _rx(0), _wx(0), _ms(ms), _full(false), _has_cv(false), _closed(0) {
        _buf = (char*) co::alloc(_buf_size);
    }

    ~pipe_impl() {
        co::free(_buf, _buf_size);
        if (_has_cv) xx::cv_free(&_cv);
    }

    void read(void* p);
    void write(void* p, int v);
    bool done() const { return g_done; }
    void close();
    bool is_closed() const { return atomic_load(&_closed, mo_relaxed); }

    struct waitx {
        co::Coroutine* co;
        union {
            uint8 state;
            struct {
                uint8 state;
                uint8 done; // 1: ok, 2: channel closed
                uint8 v;
            } x;
            void* dummy;
        };
        void* buf;
        size_t len; // total length of the memory
    };

    waitx* create_waitx(co::Coroutine* co, void* buf) {
        waitx* w;
        if (co && gSched->on_stack(buf)) {
            w = (waitx*) co::alloc(sizeof(waitx) + _blk_size);
            w->buf = (char*)w + sizeof(waitx);
            w->len = sizeof(waitx) + _blk_size;
        } else {
            w = (waitx*) co::alloc(sizeof(waitx));
            w->buf = buf;
            w->len = sizeof(waitx);
        }
        w->co = co;
        w->state = st_wait;
        w->x.done = 0;
        return w;
    }

  private:
    void _read_block(void* p) {
        _d(p);
        _c(p, _buf + _rx, 1);
        _d(_buf + _rx);
        _rx += _blk_size;
        if (_rx == _buf_size) _rx = 0;
    }

    void _write_block(void* p, int v) {
        _c(_buf + _wx, p, v);
        _wx += _blk_size;
        if (_wx == _buf_size) _wx = 0;
    }

  private:
    xx::mutex _m;
    xx::cv_t _cv;
    xx::pipe::cpmv_t _c;
    xx::pipe::destruct_t _d;
    co::deque<waitx*> _wq;
    char* _buf;       // buffer
    uint32 _buf_size; // buffer size
    uint32 _blk_size; // block size
    uint32 _rx;       // read pos
    uint32 _wx;       // write pos
    uint32 _ms;       // timeout in milliseconds
    bool _full;       // 0: not full, 1: full
    bool _has_cv;
    uint8 _closed;
};

void pipe_impl::read(void* p) {
    auto s = gSched;
    _m.lock();

    // buffer is neither empty nor full
    if (_rx != _wx) {
        this->_read_block(p);
        _m.unlock();
        goto done;
    }

    // buffer is full
    if (_full) {
        this->_read_block(p);

        while (!_wq.empty()) {
            waitx* w = _wq.front(); // wait for write
            _wq.pop_front();

            // TODO: is mo_relaxed safe here?
            if (atomic_bool_cas(&w->state, st_wait, st_ready, mo_relaxed, mo_relaxed)) {
                this->_write_block(w->buf, w->x.v & 1);
                if (w->x.v & 2) _d(w->buf);
                w->x.done = 1;
                if (w->co) {
                    _m.unlock();
                    ((co::SchedulerImpl*) w->co->s)->add_ready_task(w->co);
                } else {
                    _m.unlock();
                    xx::cv_notify_all(&_cv);
                }
                goto done;

            } else { /* timeout */
                if (w->x.v & 2) _d(w->buf);
                co::free(w, w->len);
            }
        }

        _full = false;
        _m.unlock();
        goto done;
    }

    // buffer is empty
    if (this->is_closed()) { _m.unlock(); goto enod; }
    if (s) {
        auto co = s->running();
        waitx* w = this->create_waitx(co, p);
        w->x.v = (w->buf != p ? 2 : 0);
        _wq.push_back(w);
        _m.unlock();

        if (co->s != s) co->s = s;
        co->waitx = (co::waitx_t*)w;

        if (_ms != (uint32)-1) s->add_timer(_ms);
        s->yield();

        co->waitx = 0;
        if (!s->timeout()) {
            if (w->x.done == 1) {
                if (w->buf != p) {
                    _d(p);
                    _c(p, w->buf, 1); // mv
                    _d(w->buf);
                }
                co::free(w, w->len);
                goto done;
            }

            assert(w->x.done == 2); // channel closed
            co::free(w, w->len);
            goto enod;
        }
        goto enod;

    } else {
        bool tmout = false;
        waitx* w = this->create_waitx(NULL, p);
        _wq.push_back(w);
        if (!_has_cv) { xx::cv_init(&_cv); _has_cv = true; }

        for (;;) {
            if (_ms == (uint32)-1) {
                xx::cv_wait(&_cv, _m.native_handle());
            } else {
                tmout = !xx::cv_wait(&_cv, _m.native_handle(), _ms);
            }
            if (!tmout || !atomic_bool_cas(&w->state, st_wait, st_timeout, mo_relaxed, mo_relaxed)) {
                const auto x = w->x.done;
                if (x) {
                    _m.unlock();
                    co::free(w, w->len);
                    if (x == 1) goto done;
                    goto enod; // x == 2, channel closed
                }
            } else {
                _m.unlock();
                goto enod;
            }
        }
    }

  enod:
    g_done = false;
    return;
  done:
    g_done = true;
}

void pipe_impl::write(void* p, int v) {
    auto s = gSched;
    _m.lock();
    if (this->is_closed()) { _m.unlock(); goto enod; }

    // buffer is neither empty nor full
    if (_rx != _wx) {
        this->_write_block(p, v);
        if (_rx == _wx) _full = true;
        _m.unlock();
        goto done;
    } 

    // buffer is empty
    if (!_full) {
        while (!_wq.empty()) {
            waitx* w = _wq.front(); // wait for read
            _wq.pop_front();

            // TODO: is mo_relaxed safe here?
            if (atomic_bool_cas(&w->state, st_wait, st_ready, mo_relaxed, mo_relaxed)) {
                w->x.done = 1;
                if (w->co) {
                    if (!w->x.v) _d(w->buf);
                    _c(w->buf, p, v);
                    _m.unlock();
                    ((co::SchedulerImpl*) w->co->s)->add_ready_task(w->co);
                } else {
                    _d(w->buf);
                    _c(w->buf, p, v);
                    _m.unlock();
                    xx::cv_notify_all(&_cv);
                }
                goto done;

            } else { /* timeout */
                co::free(w, w->len);
            }
        }

        this->_write_block(p, v);
        if (_rx == _wx) _full = true;
        _m.unlock();
        goto done;
    }
    
    // buffer is full
    if (s) {
        auto co = s->running();
        waitx* w = this->create_waitx(co, p);
        if (w->buf != p) {
            _c(w->buf, p, v);
            w->x.v = 1 | 2;
        } else {
            w->x.v = (uint8)v;
        }
        _wq.push_back(w);
        _m.unlock();

        if (co->s != s) co->s = s;
        co->waitx = (co::waitx_t*)w;

        if (_ms != (uint32)-1) s->add_timer(_ms);
        s->yield();

        co->waitx = 0;
        if (!s->timeout()) {
            co::free(w, w->len);
            goto done;
        }
        goto enod; // timeout

    } else {
        bool tmout = false;
        waitx* w = this->create_waitx(NULL, p);
        w->x.v = (uint8)v;
        _wq.push_back(w);
        if (!_has_cv) { xx::cv_init(&_cv); _has_cv = true; }

        for (;;) {
            if (_ms == (uint32)-1) {
                xx::cv_wait(&_cv, _m.native_handle());
            } else {
                tmout = !xx::cv_wait(&_cv, _m.native_handle(), _ms);
            }
            if (!tmout || !atomic_bool_cas(&w->state, st_wait, st_timeout, mo_relaxed, mo_relaxed)) {
                if (w->x.done) {
                    assert(w->x.done == 1);
                    _m.unlock();
                    co::free(w, w->len);
                    goto done;
                }
            } else {
                _m.unlock();
                goto enod;
            }
        }
    }

  enod:
    g_done = false;
    return;
  done:
    g_done = true;
}

void pipe_impl::close() {
    const auto x = atomic_cas(&_closed, 0, 1, mo_relaxed, mo_relaxed);
    if (x == 0) {
        xx::mutex_guard g(_m);
        if (_rx == _wx && !_full) { /* empty */
            while (!_wq.empty()) {
                waitx* w = _wq.front(); // wait for read
                _wq.pop_front();

                if (atomic_bool_cas(&w->state, st_wait, st_ready, mo_relaxed, mo_relaxed)) {
                    w->x.done = 2; // channel closed
                    if (w->co) {
                        ((co::SchedulerImpl*) w->co->s)->add_ready_task(w->co);
                    } else {
                        xx::cv_notify_all(&_cv);
                    }
                } else {
                    co::free(w, w->len);
                }
            }
        }
        atomic_store(&_closed, 2, mo_relaxed);

    } else if (x == 1) {
        while (atomic_load(&_closed, mo_relaxed) != 2) co::sleep(1);
    }
}

pipe::pipe(uint32 buf_size, uint32 blk_size, uint32 ms, pipe::cpmv_t&& c, pipe::destruct_t&& d) {
    _p = (uint32*) co::alloc(sizeof(pipe_impl) + 8);
    _p[0] = 1;
    new (_p + 2) pipe_impl(buf_size, blk_size, ms, std::move(c), std::move(d));
}

pipe::~pipe() {
    if (_p && atomic_dec(_p, mo_acq_rel) == 0) {
        ((pipe_impl*)(_p + 2))->~pipe_impl();
        co::free(_p, sizeof(pipe_impl) + 8);
        _p = 0;
    }
}

void pipe::read(void* p) const {
    ((pipe_impl*)(_p + 2))->read(p);
}

void pipe::write(void* p, int v) const {
    ((pipe_impl*)(_p + 2))->write(p, v);
}

void pipe::close() const {
    ((pipe_impl*)(_p + 2))->close();
}

bool pipe::is_closed() const {
    return ((pipe_impl*)(_p + 2))->is_closed();
}

bool pipe::done() const {
    return ((pipe_impl*)(_p + 2))->done();
}

} // xx

class pool_impl {
  public:
    typedef co::array<void*> V;

    pool_impl()
        : _pools(co::scheduler_num(), 0), _maxcap((size_t)-1) {
    }

    pool_impl(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap)
        : _pools(co::scheduler_num(), 0), _maxcap(cap), 
          _ccb(std::move(ccb)), _dcb(std::move(dcb)) {
    }

    ~pool_impl() { this->clear(); }

    void* pop();

    void push(void* p);

    void clear();

    size_t size() const;

  private:
    co::array<V> _pools;
    size_t _maxcap;
    std::function<void*()> _ccb;
    std::function<void(void*)> _dcb;
};

inline void* pool_impl::pop() {
    auto s = gSched;
    CHECK(s) << "must be called in coroutine..";
    auto& v = _pools[s->id()];
    return !v.empty() ? v.pop_back() : (_ccb ? _ccb() : nullptr);
}

inline void pool_impl::push(void* p) {
    if (p) {
        auto s = gSched;
        CHECK(s) << "must be called in coroutine..";
        auto& v = _pools[s->id()];
        (v.size() < _maxcap || !_dcb) ? v.push_back(p) : _dcb(p);
    }
}

// Create n coroutines to clear all the pools, n is number of schedulers.
// clear() blocks untils all the coroutines are done.
void pool_impl::clear() {
    if (co::is_active()) {
        auto& scheds = co::schedulers();
        co::wait_group wg((uint32)scheds.size());

        for (auto& s : scheds) {
            s->go([this, wg]() {
                auto& v = this->_pools[gSched->id()];
                if (this->_dcb) for (auto& e : v) this->_dcb(e);
                v.clear();
                wg.done();
            });
        }

        wg.wait();
    } else {
        for (auto& v : _pools) {
            if (this->_dcb) for (auto& e : v) this->_dcb(e);
            v.clear();
        }
    }
}

inline size_t pool_impl::size() const {
    auto s = gSched;
    CHECK(s) << "must be called in coroutine..";
    return _pools[s->id()].size();
}

// memory: |4(refn)|4|pool_impl|
pool::pool() {
    _p = (uint32*) co::alloc(sizeof(pool_impl) + 8);
    _p[0] = 1;
    new (_p + 2) pool_impl();
}

pool::~pool() {
    if (_p && atomic_dec(_p, mo_acq_rel) == 0) {
        ((pool_impl*)(_p + 2))->~pool_impl();
        co::free(_p, sizeof(pool_impl) + 8);
        _p = 0;
    }
}

pool::pool(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap) {
    _p = (uint32*) co::alloc(sizeof(pool_impl) + 8);
    _p[0] = 1;
    new (_p + 2) pool_impl(std::move(ccb), std::move(dcb), cap);
}

void* pool::pop() const {
    return ((pool_impl*)(_p + 2))->pop();
}

void pool::push(void* p) const {
    ((pool_impl*)(_p + 2))->push(p);
}

void pool::clear() const {
    ((pool_impl*)(_p + 2))->clear();
}

size_t pool::size() const {
    return ((pool_impl*)(_p + 2))->size();
}

} // co
