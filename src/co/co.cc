#include "sched.h"
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

__thread uint32 g_tid;

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
    xx::mutex& _lock;
    DISALLOW_COPY_AND_ASSIGN(mutex_guard);
};

class mutex_impl {
  public:
    struct queue {
        static const int N = 12;
        struct _memb : co::clink {
            size_t size;
            uint8 rx;
            uint8 wx;
            void* q[];
        };

        _memb* _make_memb() {
            _memb* m = (_memb*) co::alloc(sizeof(_memb) + N * sizeof(void*), co::cache_line_size);
            m->size = 0;
            m->rx = 0;
            m->wx = 0;
            return m;
        }

        queue() noexcept : _m(0) {}

        ~queue() {
            for (auto h = _q.front(); h;) {
                const auto m = (_memb*)h;
                h = h->next;
                co::free(m, sizeof(_memb) + N * sizeof(void*));
            }
        }

        size_t size() const noexcept { return _m ? _m->size : 0; }
        bool empty() const noexcept { return this->size() == 0; }

        void push_back(void* x) {
            _memb* m = (_memb*) _q.back();
            if (!m || m->wx == N) {
                m = this->_make_memb();
                _q.push_back(m);
            }
            m->q[m->wx++] = x;
            ++_m->size;
        }

        void* pop_front() {
            void* x = 0;
            if (_m && _m->rx < _m->wx) {
                x = _m->q[_m->rx++];
                --_m->size;
                if (_m->rx == _m->wx) {
                    _m->rx = _m->wx = 0;
                    if (_q.back() != _m) {
                        _memb* const m = (_memb*) _q.pop_front();
                        _m->size = m->size;
                        co::free(m, sizeof(_memb) + N * sizeof(void*));
                    }
                }
            }
            return x;
        }

        union {
            _memb* _m;
            co::clist _q;
        };
    };

    mutex_impl() : _refn(1), _lock(0), _has_cv(false) {}
    ~mutex_impl() { if (_has_cv) xx::cv_free(&_cv); }

    void lock();
    void unlock();
    bool try_lock();

    void ref() { atomic_inc(&_refn, mo_relaxed); }
    uint32 unref() { return atomic_dec(&_refn, mo_acq_rel); }

  private:
    xx::mutex _m;
    xx::cv_t _cv;
    queue _wq;
    uint32 _refn;
    uint8 _lock;
    bool _has_cv;
};

inline bool mutex_impl::try_lock() {
    xx::mutex_guard g(_m);
    return _lock ? false : (_lock = 1);
}

void mutex_impl::lock() {
    const auto sched = xx::gSched;
    if (sched) { /* in coroutine */
        _m.lock();
        if (!_lock) {
            _lock = 1;
            _m.unlock();
        } else {
            Coroutine* const co = sched->running();
            _wq.push_back(co);
            _m.unlock();
            sched->yield();
        }

    } else { /* non-coroutine */
        xx::mutex_guard g(_m);
        if (!_lock) {
            _lock = 1;
        } else {
            _wq.push_back(nullptr);
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
    if (_wq.empty()) {
        _lock = 0;
        _m.unlock();
    } else {
        Coroutine* const co = (Coroutine*) _wq.pop_front();
        if (co) {
            _m.unlock();
            co->sched->add_ready_task(co);
        } else {
            _lock = 2;
            _m.unlock();
            xx::cv_notify_one(&_cv);
        }
    }
}

class event_impl {
  public:
    event_impl(bool m, bool s, uint32 wg=0)
        : _wt(0), _sn(0), _refn(1), _wg(wg), _signaled(s), _manual_reset(m), _has_cv(false) {
    }
    ~event_impl() { if (_has_cv) xx::cv_free(&_cv); }

    bool wait(uint32 ms);
    void signal();
    void reset();

    void ref() { atomic_inc(&_refn, mo_relaxed); }
    uint32 unref() { return atomic_dec(&_refn, mo_acq_rel); }
    uint32* wg() noexcept { return &_wg; }

  private:
    xx::mutex _m;
    xx::cv_t _cv;
    co::clist _wc;
    uint32 _wt;
    uint32 _sn;
    uint32 _refn;
    uint32 _wg; // for wait group
    bool _signaled;
    const bool _manual_reset;
    bool _has_cv;
};

bool event_impl::wait(uint32 ms) {
    const auto sched = gSched;
    if (sched) { /* in coroutine */
        Coroutine* co = sched->running();
        {
            xx::mutex_guard g(_m);
            if (_signaled) {
                if (!_manual_reset) _signaled = false;
                return true;
            }
            if (ms == 0) return false;

            waitx_t* x = 0;
            while (!_wc.empty()) {
                waitx_t* const w = (waitx_t*) _wc.front();
                if (w->state != st_timeout) break;
                _wc.pop_front();
                !x ? (void)(x = w) : co::free(w, sizeof(*w));
            }
            x ? (void)(x->state = st_wait) : (void)(x = make_waitx(co));
            co->waitx = x;
            _wc.push_back(x);
        }

        if (ms != (uint32)-1) sched->add_timer(ms);
        sched->yield();
        if (!sched->timeout()) co::free(co->waitx, sizeof(waitx_t));
        co->waitx = nullptr;
        return !sched->timeout();

    } else { /* not in coroutine */
        xx::mutex_guard g(_m);
        if (_signaled) {
            if (!_manual_reset) _signaled = false;
            return true;
        }
        if (ms == 0) return false;

        const uint32 sn = _sn;
        ++_wt;
        if (!_has_cv) { xx::cv_init(&_cv); _has_cv = true; }
        if (ms != (uint32)-1) {
            const bool r = xx::cv_wait(&_cv, _m.native_handle(), ms);
            if (!r && sn == _sn) { assert(_wt > 0); --_wt; }
            return r;
        } else {
            xx::cv_wait(&_cv, _m.native_handle());
            return true;
        }
    }
}

void event_impl::signal() {
    co::clink* h = 0;
    {
        bool has_wt = false, has_wc = false;
        xx::mutex_guard g(_m);
        if (_wt > 0) {
            _wt = 0;
            has_wt = true;
        }

        if (!_wc.empty()) {
            h = _wc.front();
            _wc.clear();
            if (!has_wt) {
                do {
                    waitx_t* const w = (waitx_t*)h;
                    h = h->next;
                    if (atomic_bool_cas(&w->state, st_wait, st_ready, mo_relaxed, mo_relaxed)) {
                        has_wc = true;
                        w->co->sched->add_ready_task(w->co);
                        break;
                    } else { /* timeout */
                        co::free(w, sizeof(*w));
                    }
                } while (h);
            }
        }

        if (has_wt || has_wc) {
            if (_signaled && !_manual_reset) _signaled = false;
            if (has_wt) { ++_sn; xx::cv_notify_all(&_cv); }
        } else {
            if (!_signaled ) _signaled = true;
        }
    }

    while (h) {
        waitx_t* const w = (waitx_t*) h;
        h = h->next;
        if (atomic_bool_cas(&w->state, st_wait, st_ready, mo_relaxed, mo_relaxed)) {
            w->co->sched->add_ready_task(w->co);
        } else { /* timeout */
            co::free(w, sizeof(*w));
        }
    }
}

inline void event_impl::reset() {
    xx::mutex_guard g(_m);
    _signaled = false;
}

class sync_event_impl {
  public:
    explicit sync_event_impl(bool m, bool s)
        : _wt(0), _sn(0), _signaled(s), _manual_reset(m) {
        xx::cv_init(&_cv);
    }

    ~sync_event_impl() {
        xx::cv_free(&_cv);
    }

    void wait() {
        xx::mutex_guard g(_m);
        if (_signaled) {
            if (!_manual_reset) _signaled = false;
            return;
        }
        ++_wt;
        xx::cv_wait(&_cv, _m.native_handle());
    }

    bool wait(uint32 ms) {
        xx::mutex_guard g(_m);
        if (_signaled) {
            if (!_manual_reset) _signaled = false;
            return true;
        }
        if (ms == 0) return false;

        const uint32 sn = _sn;
        ++_wt;
        const bool r = co::xx::cv_wait(&_cv, _m.native_handle(), ms);
        if (!r && sn == _sn) { assert(_wt > 0); --_wt; }
        return r;
    }

    void signal() {
        xx::mutex_guard g(_m);
        if (_wt > 0) {
            _wt = 0;
            if (_signaled && !_manual_reset) _signaled = false;
            ++_sn;
            xx::cv_notify_all(&_cv);
        } else {
            if (!_signaled ) _signaled = true;
        }
    }

    void reset() {
        xx::mutex_guard g(_m);
        _signaled = false;
    }

  private:
    xx::mutex _m;
    xx::cv_t _cv;
    uint32 _wt;
    uint32 _sn;
    bool _signaled;
    const bool _manual_reset;
};

__thread bool g_done = false;

class pipe_impl {
  public:
    pipe_impl(uint32 buf_size, uint32 blk_size, uint32 ms, pipe::C&& c, pipe::D&& d)
        : _buf_size(buf_size), _blk_size(blk_size), _ms(ms), _has_cv(false),
          _c(std::move(c)), _d(std::move(d)), _rx(0), _wx(0), _refn(1), _full(0), _closed(0) {
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

    void ref() { atomic_inc(&_refn, mo_relaxed); }
    uint32 unref() { return atomic_dec(&_refn, mo_acq_rel); }

    struct waitx : co::clink {
        Coroutine* co;
        union {
            uint8 state;
            struct {
                uint8 state;
                uint8 done; // 1: ok, 2: channel closed
                uint8 v; // 0: cp, 1: mv, 2: need destruct the object in buf
            } x;
            void* dummy;
        };
        void* buf;
        size_t len; // total length of the memory
    };

    waitx* create_waitx(Coroutine* co, void* buf) {
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
    void _read_block(void* p);
    void _write_block(void* p, int v);

  private:
    char* _buf;       // buffer
    uint32 _buf_size; // buffer size
    uint32 _blk_size; // block size
    uint32 _ms;       // timeout in milliseconds
    bool _has_cv;
    xx::pipe::C _c;
    xx::pipe::D _d;

    xx::mutex _m;
    xx::cv_t _cv;
    co::clist _wq;
    uint32 _rx; // read pos
    uint32 _wx; // write pos
    uint32 _refn;
    uint8 _full;
    uint8 _closed;
};

inline void pipe_impl::_read_block(void* p) {
    _d(p);
    _c(p, _buf + _rx, 1);
    _d(_buf + _rx);
    _rx += _blk_size;
    if (_rx == _buf_size) _rx = 0;
}

inline void pipe_impl::_write_block(void* p, int v) {
    _c(_buf + _wx, p, v);
    _wx += _blk_size;
    if (_wx == _buf_size) _wx = 0;
}

void pipe_impl::read(void* p) {
    auto sched = gSched;
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
            waitx* w = (waitx*) _wq.pop_front(); // wait for write
            if (_ms == (uint32)-1 || atomic_bool_cas(&w->state, st_wait, st_ready, mo_relaxed, mo_relaxed)) {
                this->_write_block(w->buf, w->x.v & 1);
                if (w->x.v & 2) _d(w->buf);
                w->x.done = 1;
                if (w->co) {
                    _m.unlock();
                    w->co->sched->add_ready_task(w->co);
                } else {
                    xx::cv_notify_all(&_cv);
                    _m.unlock();
                }
                goto done;

            } else { /* timeout */
                if (w->x.v & 2) _d(w->buf);
                co::free(w, w->len);
            }
        }

        _full = 0;
        _m.unlock();
        goto done;
    }

    // buffer is empty
    if (this->is_closed()) { _m.unlock(); goto enod; }
    if (sched) {
        auto co = sched->running();
        waitx* w = this->create_waitx(co, p);
        w->x.v = (w->buf != p ? 0 : 2);
        _wq.push_back(w);
        _m.unlock();

        co->waitx = (waitx_t*)w;
        if (_ms != (uint32)-1) sched->add_timer(_ms);
        sched->yield();

        co->waitx = 0;
        if (!sched->timeout()) {
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
        bool r = true;
        waitx* w = this->create_waitx(NULL, p);
        _wq.push_back(w);
        if (!_has_cv) { xx::cv_init(&_cv); _has_cv = true; }

        for (;;) {
            if (_ms == (uint32)-1) {
                xx::cv_wait(&_cv, _m.native_handle());
            } else {
                r = xx::cv_wait(&_cv, _m.native_handle(), _ms);
            }
            if (r || !atomic_bool_cas(&w->state, st_wait, st_timeout, mo_relaxed, mo_relaxed)) {
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
    auto sched = gSched;
    _m.lock();
    if (this->is_closed()) { _m.unlock(); goto enod; }

    // buffer is neither empty nor full
    if (_rx != _wx) {
        this->_write_block(p, v);
        if (_rx == _wx) _full = 1;
        _m.unlock();
        goto done;
    } 

    // buffer is empty
    if (!_full) {
        while (!_wq.empty()) {
            waitx* w = (waitx*) _wq.pop_front(); // wait for read
            if (_ms == (uint32)-1 || atomic_bool_cas(&w->state, st_wait, st_ready, mo_relaxed, mo_relaxed)) {
                w->x.done = 1;
                if (w->co) {
                    if (w->x.v & 2) _d(w->buf);
                    _c(w->buf, p, v);
                    _m.unlock();
                    w->co->sched->add_ready_task(w->co);
                } else {
                    _d(w->buf);
                    _c(w->buf, p, v);
                    xx::cv_notify_all(&_cv);
                    _m.unlock();
                }
                goto done;

            } else { /* timeout */
                co::free(w, w->len);
            }
        }

        this->_write_block(p, v);
        if (_rx == _wx) _full = 1;
        _m.unlock();
        goto done;
    }
    
    // buffer is full
    if (sched) {
        auto co = sched->running();
        waitx* w = this->create_waitx(co, p);
        if (w->buf != p) { /* p is on the coroutine stack */
            _c(w->buf, p, v);
            w->x.v = 1 | 2;
        } else {
            w->x.v = (uint8)v;
        }
        _wq.push_back(w);
        _m.unlock();

        co->waitx = (waitx_t*)w;
        if (_ms != (uint32)-1) sched->add_timer(_ms);
        sched->yield();

        co->waitx = 0;
        if (!sched->timeout()) {
            co::free(w, w->len);
            goto done;
        }
        goto enod; // timeout

    } else {
        bool r = true;
        waitx* w = this->create_waitx(NULL, p);
        w->x.v = (uint8)v;
        _wq.push_back(w);
        if (!_has_cv) { xx::cv_init(&_cv); _has_cv = true; }

        for (;;) {
            if (_ms == (uint32)-1) {
                xx::cv_wait(&_cv, _m.native_handle());
            } else {
                r = xx::cv_wait(&_cv, _m.native_handle(), _ms);
            }
            if (r || !atomic_bool_cas(&w->state, st_wait, st_timeout, mo_relaxed, mo_relaxed)) {
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
                waitx* w = (waitx*) _wq.pop_front(); // wait for read
                if (atomic_bool_cas(&w->state, st_wait, st_ready, mo_relaxed, mo_relaxed)) {
                    w->x.done = 2; // channel closed
                    if (w->co) {
                        w->co->sched->add_ready_task(w->co);
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

pipe::pipe(uint32 buf_size, uint32 blk_size, uint32 ms, pipe::C&& c, pipe::D&& d) {
    _p = co::alloc(sizeof(pipe_impl), co::cache_line_size);
    new (_p) pipe_impl(buf_size, blk_size, ms, std::move(c), std::move(d));
}

pipe::pipe(const pipe& p) : _p(p._p) {
    if (_p) god::cast<pipe_impl*>(_p)->ref();
}

pipe::~pipe() {
    const auto p = (pipe_impl*)_p;
    if (p && p->unref() == 0) {
        p->~pipe_impl();
        co::free(_p, sizeof(pipe_impl));
        _p = 0;
    }
}

void pipe::read(void* p) const {
    god::cast<pipe_impl*>(_p)->read(p);
}

void pipe::write(void* p, int v) const {
    god::cast<pipe_impl*>(_p)->write(p, v);
}

bool pipe::done() const {
    return god::cast<pipe_impl*>(_p)->done();
}

void pipe::close() const {
    god::cast<pipe_impl*>(_p)->close();
}

bool pipe::is_closed() const {
    return god::cast<pipe_impl*>(_p)->is_closed();
}

class pool_impl {
  public:
    typedef co::vector<void*> V;

    pool_impl()
        : _maxcap((size_t)-1), _refn(1) {
        this->_make_pools();
    }

    pool_impl(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap)
        : _maxcap(cap), _refn(1), _ccb(std::move(ccb)), _dcb(std::move(dcb)) {
        this->_make_pools();
    }

    ~pool_impl() {
        this->clear();
        this->_free_pools();
    }

    void* pop();
    void push(void* p);
    void clear();
    size_t size() const;

    void _make_pools() {
        _size = co::sched_num();
        _pools = (V*) co::zalloc(sizeof(V) * _size);
    }

    void _free_pools() {
        for (int i = 0; i < _size; ++i) _pools[i].~V();
        co::free(_pools, sizeof(V) * _size);
    }

    void ref() { atomic_inc(&_refn, mo_relaxed); }
    uint32 unref() { return atomic_dec(&_refn, mo_acq_rel); }

  private:
    V* _pools;
    size_t _size;
    size_t _maxcap;
    uint32 _refn;
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
    if (xx::is_active()) {
        auto& scheds = co::scheds();
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
        for (size_t i = 0; i < _size; ++i) {
            auto& v = _pools[i];
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

} // xx

mutex::mutex() {
    _p = co::alloc(sizeof(xx::mutex_impl), co::cache_line_size);
    new (_p) xx::mutex_impl();
}

mutex::mutex(const mutex& m) : _p(m._p) {
    if (_p) god::cast<xx::mutex_impl*>(_p)->ref();
}

mutex::~mutex() {
    const auto p = (xx::mutex_impl*)_p;
    if (p && p->unref() == 0) {
        p->~mutex_impl();
        co::free(_p, sizeof(xx::mutex_impl));
        _p = 0;
    }
}

void mutex::lock() const {
    god::cast<xx::mutex_impl*>(_p)->lock();
}

void mutex::unlock() const {
    god::cast<xx::mutex_impl*>(_p)->unlock();
}

bool mutex::try_lock() const {
    return god::cast<xx::mutex_impl*>(_p)->try_lock();
}


event::event(bool manual_reset, bool signaled) {
    _p = co::alloc(sizeof(xx::event_impl), co::cache_line_size);
    new (_p) xx::event_impl(manual_reset, signaled);
}

event::event(const event& e) : _p(e._p) {
    if (_p) god::cast<xx::event_impl*>(_p)->ref();
}

event::~event() {
    const auto p = (xx::event_impl*)_p;
    if (p && p->unref() == 0) {
        p->~event_impl();
        co::free(_p, sizeof(xx::event_impl));
        _p = 0;
    }
}

bool event::wait(uint32 ms) const {
    return god::cast<xx::event_impl*>(_p)->wait(ms);
}

void event::signal() const {
    god::cast<xx::event_impl*>(_p)->signal();
}

void event::reset() const {
    god::cast<xx::event_impl*>(_p)->reset();
}


sync_event::sync_event(bool manual_reset, bool signaled) {
    _p = co::alloc(sizeof(xx::sync_event_impl), co::cache_line_size);
    new (_p) xx::sync_event_impl(manual_reset, signaled);
}

sync_event::~sync_event() {
    if (_p) {
        ((xx::sync_event_impl*)_p)->~sync_event_impl();
        co::free(_p, sizeof(xx::sync_event_impl));
        _p = 0;
    }
}

void sync_event::signal() {
    ((xx::sync_event_impl*)_p)->signal();
}

void sync_event::reset() {
    ((xx::sync_event_impl*)_p)->reset();
}

void sync_event::wait() {
    ((xx::sync_event_impl*)_p)->wait();
}

bool sync_event::wait(uint32 ms) {
    return ((xx::sync_event_impl*)_p)->wait(ms);
}


wait_group::wait_group(uint32 n) {
    _p = co::alloc(sizeof(xx::event_impl), co::cache_line_size);
    new (_p) xx::event_impl(false, false, n);
}

wait_group::wait_group(const wait_group& wg) : _p(wg._p) {
    if (_p) god::cast<xx::event_impl*>(_p)->ref();
}

wait_group::~wait_group() {
    const auto p = (xx::event_impl*)_p;
    if (p && p->unref() == 0) {
        p->~event_impl();
        co::free(_p, sizeof(xx::event_impl));
        _p = 0;
    }
}

void wait_group::add(uint32 n) const {
    atomic_add(god::cast<xx::event_impl*>(_p)->wg(), n, mo_relaxed);
}

void wait_group::done() const {
    const auto e = god::cast<xx::event_impl*>(_p);
    const uint32 x = atomic_dec(e->wg(), mo_acq_rel);
    CHECK(x != (uint32)-1);
    if (x == 0) e->signal();
}

void wait_group::wait() const {
    god::cast<xx::event_impl*>(_p)->wait((uint32)-1);
}


pool::pool() {
    _p = co::alloc(sizeof(xx::pool_impl), co::cache_line_size);
    new (_p) xx::pool_impl();
}

pool::pool(const pool& p) : _p(p._p) {
    if (_p) god::cast<xx::pool_impl*>(_p)->ref();
}

pool::~pool() {
    const auto p = (xx::pool_impl*)_p;
    if (p && p->unref() == 0) {
        p->~pool_impl();
        co::free(_p, sizeof(xx::pool_impl));
        _p = 0;
    }
}

pool::pool(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap) {
    _p = co::alloc(sizeof(xx::pool_impl));
    new (_p) xx::pool_impl(std::move(ccb), std::move(dcb), cap);
}

void* pool::pop() const {
    return god::cast<xx::pool_impl*>(_p)->pop();
}

void pool::push(void* p) const {
    god::cast<xx::pool_impl*>(_p)->push(p);
}

void pool::clear() const {
    god::cast<xx::pool_impl*>(_p)->clear();
}

size_t pool::size() const {
    return god::cast<xx::pool_impl*>(_p)->size();
}

} // co
