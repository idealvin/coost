#include "scheduler.h"
#include "co/stl.h"

namespace co {

class EventImpl {
  public:
    EventImpl(bool manual_reset, bool signaled)
        : _count(0), _manual_reset(manual_reset), _signaled(signaled), _has_cond(false) {
    }
    ~EventImpl() { if (_has_cond) co::xx::cond_destroy(&_cond); }

    bool wait(uint32 ms);

    void signal();

    void reset();

  private:
    ::Mutex _mtx;
    co::xx::cond_t _cond;
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
    bool _has_cond;
};

bool EventImpl::wait(uint32 ms) {
    auto s = gSched;
    if (s) { /* in coroutine */
        Coroutine* co = s->running();
        if (co->s != s) co->s = s;
        {
            ::MutexGuard g(_mtx);
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
                ::MutexGuard g(_mtx);
                --_wait.nco;
                if (!_manual_reset && _count == 0) _signaled = false;
            }
            co::free(co->waitx, sizeof(co::waitx_t));
        } else {
            bool erased;
            {
                ::MutexGuard g(_mtx);
                --_wait.nco;
                if (_signaled && !_manual_reset && _count == 0) _signaled = false;
                erased = _co_wait.erase(co->waitx) == 1;
            }
            if (erased) co::free(co->waitx, sizeof(co::waitx_t));
        }

        co->waitx = nullptr;
        return !s->timeout();

    } else { /* not in coroutine */
        ::MutexGuard g(_mtx);
        if (!_signaled) {
            ++_wait.nth;
            if (!_has_cond) { co::xx::cond_init(&_cond); _has_cond = true; }
            bool r = true;
            if (ms == (uint32)-1) {
                co::xx::cond_wait(&_cond, _mtx.mutex());
            } else {
                r = co::xx::cond_wait(&_cond, _mtx.mutex(), ms);
            }
            --_wait.nth;
            if (!r) return false;
            assert(_signaled);
        }
        if (!_manual_reset && _count == 0) _signaled = false;
        return true;
    }
}

void EventImpl::signal() {
    co::hash_set<co::waitx_t*> co_wait;
    {
        ::MutexGuard g(_mtx);
        if (!_co_wait.empty()) _co_wait.swap(co_wait);
        if (!_signaled) {
            _signaled = true;
            if (_wait.nth > 0) {
                if (!_has_cond) { co::xx::cond_init(&_cond); _has_cond = true; }
                co::xx::cond_notify_all(&_cond);
            }
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

inline void EventImpl::reset() {
    ::MutexGuard g(_mtx);
    _signaled = false;
}

// memory: |4(refn)|4|EventImpl|
Event::Event(bool manual_reset, bool signaled) {
    _p = (uint32*) co::alloc(sizeof(EventImpl) + 8);
    _p[0] = 1;
    new (_p + 2) EventImpl(manual_reset, signaled);
}

Event::~Event() {
    if (_p && atomic_dec(_p, mo_acq_rel) == 0) {
        ((EventImpl*)(_p + 2))->~EventImpl();
        co::free(_p, sizeof(EventImpl) + 8);
    }
}

bool Event::wait(uint32 ms) const {
    return ((EventImpl*)(_p + 2))->wait(ms);
}

void Event::signal() const {
    ((EventImpl*)(_p + 2))->signal();
}

void Event::reset() const {
    ((EventImpl*)(_p + 2))->reset();
}

// memory: |4(refn)|4(counter)|EventImpl|
WaitGroup::WaitGroup(uint32 n) {
    _p = (uint32*) co::alloc(sizeof(EventImpl) + 8);
    _p[0] = 1; // refn
    _p[1] = n; // counter
    new (_p + 2) EventImpl(false, false);
}

WaitGroup::~WaitGroup() {
    if (_p && atomic_dec(_p, mo_acq_rel) == 0) {
        ((EventImpl*)(_p + 2))->~EventImpl();
        co::free(_p, sizeof(EventImpl) + 8);
        _p = 0;
    }
}

void WaitGroup::add(uint32 n) const {
    atomic_add(_p + 1, n, mo_relaxed);
}

void WaitGroup::done() const {
    const uint32 x = atomic_dec(_p + 1, mo_acq_rel);
    CHECK(x != (uint32)-1);
    if (x == 0) ((EventImpl*)(_p + 2))->signal();
}

void WaitGroup::wait() const {
    ((EventImpl*)(_p + 2))->wait((uint32)-1);
}

class MutexImpl {
  public:
    MutexImpl() : _lock(0), _has_cond(false) {}
    ~MutexImpl() { if (_has_cond) co::xx::cond_destroy(&_cond); }

    void lock();

    void unlock();

    bool try_lock();

  private:
    ::Mutex _mtx;
    co::xx::cond_t _cond;
    co::deque<Coroutine*> _co_wait;
    int32 _lock;
    bool _has_cond;
};

inline bool MutexImpl::try_lock() {
    ::MutexGuard g(_mtx);
    return _lock ? false : (_lock = 1);
}

void MutexImpl::lock() {
    auto s = gSched;
    if (s) { /* in coroutine */
        _mtx.lock();
        if (!_lock) {
            _lock = 1;
            _mtx.unlock();
        } else {
            Coroutine* co = s->running();
            if (co->s != s) co->s = s;
            _co_wait.push_back(co);
            _mtx.unlock();
            s->yield();
        }

    } else { /* non-coroutine */
        ::MutexGuard g(_mtx);
        if (!_lock) {
            _lock = 1;
        } else {
            _co_wait.push_back(nullptr);
            if (!_has_cond) { co::xx::cond_init(&_cond); _has_cond = true; }
            for (;;) {
                co::xx::cond_wait(&_cond, _mtx.mutex());
                if (_lock == 2) { _lock = 1; break; }
            }
        }
    }
}

void MutexImpl::unlock() {
    _mtx.lock();
    if (_co_wait.empty()) {
        _lock = 0;
        _mtx.unlock();
    } else {
        Coroutine* co = _co_wait.front();
        _co_wait.pop_front();
        if (co) {
            _mtx.unlock();
            ((SchedulerImpl*)co->s)->add_ready_task(co);
        } else {
            if (!_has_cond) { co::xx::cond_init(&_cond); _has_cond = true; }
            _lock = 2;
            _mtx.unlock();
            co::xx::cond_notify_one(&_cond);
        }
    }
}

// memory: |4(refn)|4|MutexImpl|
Mutex::Mutex() {
    _p = (uint32*) co::alloc(sizeof(MutexImpl) + 8);
    _p[0] = 1; // refn
    new (_p + 2) MutexImpl();
}

Mutex::~Mutex() {
    if (_p && atomic_dec(_p, mo_acq_rel) == 0) {
        ((MutexImpl*)(_p + 2))->~MutexImpl();
        co::free(_p, sizeof(MutexImpl) + 8);
    }
}

void Mutex::lock() const {
    ((MutexImpl*)(_p + 2))->lock();
}

void Mutex::unlock() const {
    ((MutexImpl*)(_p + 2))->unlock();
}

bool Mutex::try_lock() const {
    return ((MutexImpl*)(_p + 2))->try_lock();
}

class PoolImpl {
  public:
    typedef co::array<void*> V;

    PoolImpl()
        : _pools(co::scheduler_num(), nullptr), _maxcap((size_t)-1) {
    }

    PoolImpl(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap)
        : _pools(co::scheduler_num(), nullptr), _maxcap(cap), 
          _ccb(std::move(ccb)), _dcb(std::move(dcb)) {
    }

    ~PoolImpl() { this->clear(); }

    void* pop();

    void push(void* p);

    void clear();

    size_t size() const;

  private:
    co::array<V*> _pools;
    size_t _maxcap;
    std::function<void*()> _ccb;
    std::function<void(void*)> _dcb;
};

inline void* PoolImpl::pop() {
    CHECK(gSched) << "must be called in coroutine..";
    auto& v = _pools[gSched->id()];
    if (!v) { v = co::make<V>(1024); assert(v); }
    if (!v->empty()) {
        return v->pop_back();
    } else {
        return _ccb ? _ccb() : 0;
    }
}

inline void PoolImpl::push(void* p) {
    if (!p) return; // ignore null pointer
    CHECK(gSched) << "must be called in coroutine..";
    auto& v = _pools[gSched->id()];
    if (!v) { v = co::make<V>(1024); assert(v); }
    if (v->size() < _maxcap || !_dcb) {
        v->push_back(p);
    } else {
        _dcb(p);
    }
}

// Create n coroutines to clear all the pools, n is number of schedulers.
// clear() blocks untils all the coroutines are done.
void PoolImpl::clear() {
    if (co::is_active()) {
        auto& scheds = co::schedulers();
        WaitGroup wg;
        wg.add((uint32)scheds.size());

        for (auto& s : scheds) {
            s->go([this, wg]() {
                auto& v = this->_pools[gSched->id()];
                if (v) {
                    if (this->_dcb) for (auto& e : *v) this->_dcb(e);
                    co::del(v); v = nullptr;
                }
                wg.done();
            });
        }

        wg.wait();
    } else {
        for (auto& v : _pools) {
            if (v) {
                if (this->_dcb) for (auto& e : *v) this->_dcb(e);
                co::del(v); v = nullptr;
            }
        }
    }
}

inline size_t PoolImpl::size() const {
    CHECK(gSched) << "must be called in coroutine..";
    auto& v = _pools[gSched->id()];
    return v ? v->size() : 0;
}

// memory: |4(refn)|4|PoolImpl|
Pool::Pool() {
    _p = (uint32*) co::alloc(sizeof(PoolImpl) + 8);
    _p[0] = 1;
    new (_p + 2) PoolImpl();
}

Pool::~Pool() {
    if (_p && atomic_dec(_p, mo_acq_rel) == 0) {
        ((PoolImpl*)(_p + 2))->~PoolImpl();
        co::free(_p, sizeof(PoolImpl) + 8);
    }
}

Pool::Pool(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap) {
    _p = (uint32*) co::alloc(sizeof(PoolImpl) + 8);
    _p[0] = 1;
    new (_p + 2) PoolImpl(std::move(ccb), std::move(dcb), cap);
}

void* Pool::pop() const {
    return ((PoolImpl*)(_p + 2))->pop();
}

void Pool::push(void* p) const {
    ((PoolImpl*)(_p + 2))->push(p);
}

void Pool::clear() const {
    ((PoolImpl*)(_p + 2))->clear();
}

size_t Pool::size() const {
    return ((PoolImpl*)(_p + 2))->size();
}

namespace xx {

class PipeImpl {
  public:
    PipeImpl(uint32 buf_size, uint32 blk_size, uint32 ms)
        : _buf_size(buf_size), _blk_size(blk_size), 
          _rx(0), _wx(0), _ms(ms), _full(false) {
        _buf = (char*) co::alloc(_buf_size);
    }

    ~PipeImpl() {
        co::free(_buf, _buf_size);
    }

    void read(void* p);
    void write(const void* p);

    struct waitx {
        co::Coroutine* co;
        union {
            int state;
            void* dummy;
        };
        void* buf;
        size_t len; // total length of the memory
    };

    waitx* create_waitx(co::Coroutine* co, void* buf) {
        waitx* w;
        const bool on_stack = gSched->on_stack(buf);
        if (on_stack) {
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
        return w;
    }

  private:
    ::Mutex _m;
    co::deque<waitx*> _wq;
    char* _buf;       // buffer
    uint32 _buf_size; // buffer size
    uint32 _blk_size; // block size
    uint32 _rx;       // read pos
    uint32 _wx;       // write pos
    uint32 _ms;       // timeout in milliseconds
    bool _full;       // 0: not full, 1: full
};

void PipeImpl::read(void* p) {
    auto s = gSched;
    CHECK(s) << "must be called in coroutine..";

    _m.lock();
    if (_rx != _wx) { /* buffer is neither empty nor full */
        assert(!_full);
        assert(_wq.empty());
        memcpy(p, _buf + _rx, _blk_size);
        _rx += _blk_size;
        if (_rx == _buf_size) _rx = 0;
        _m.unlock();

    } else {
        if (!_full) { /* buffer is empty */
            auto co = s->running();
            waitx* w = this->create_waitx(co, p);
            _wq.push_back(w);
            _m.unlock();

            if (co->s != s) co->s = s;
            co->waitx = (co::waitx_t*)w;

            if (_ms != (uint32)-1) s->add_timer(_ms);
            s->yield();

            if (!s->timeout()) {
                if (w->buf != p) memcpy(p, w->buf, _blk_size);
                co::free(w, w->len);
            }

            co->waitx = 0;

        } else { /* buffer is full */
            memcpy(p, _buf + _rx, _blk_size);
            _rx += _blk_size;
            if (_rx == _buf_size) _rx = 0;

            while (!_wq.empty()) {
                waitx* w = _wq.front(); // wait for write
                _wq.pop_front();

                // TODO: is mo_relaxed safe here?
                if (atomic_bool_cas(&w->state, st_wait, st_ready, mo_relaxed, mo_relaxed)) {
                    memcpy(_buf + _wx, w->buf, _blk_size);
                    _wx += _blk_size;
                    if (_wx == _buf_size) _wx = 0;
                    _m.unlock();

                    ((co::SchedulerImpl*) w->co->s)->add_ready_task(w->co);
                    return;

                } else { /* timeout */
                    co::free(w, w->len);
                }
            }

            _full = false;
            _m.unlock();
        }
    }
}

void PipeImpl::write(const void* p) {
    auto s = gSched;
    CHECK(s) << "must be called in coroutine..";

    _m.lock();
    if (_rx != _wx) { /* buffer is neither empty nor full */
        assert(!_full);
        assert(_wq.empty());
        memcpy(_buf + _wx, p, _blk_size);
        _wx += _blk_size;
        if (_wx == _buf_size) _wx = 0;
        if (_rx == _wx) _full = true;
        _m.unlock();

    } else {
        if (!_full) { /* buffer is empty */
            while (!_wq.empty()) {
                waitx* w = _wq.front(); // wait for read
                _wq.pop_front();

                // TODO: is mo_relaxed safe here?
                if (atomic_bool_cas(&w->state, st_wait, st_ready, mo_relaxed, mo_relaxed)) {
                    _m.unlock();
                    memcpy(w->buf, p, _blk_size);
                    ((co::SchedulerImpl*) w->co->s)->add_ready_task(w->co);
                    return;
                } else { /* timeout */
                    co::free(w, w->len);
                }
            }

            memcpy(_buf + _wx, p, _blk_size);
            _wx += _blk_size;
            if (_wx == _buf_size) _wx = 0;
            if (_rx == _wx) _full = true;
            _m.unlock();

        } else { /* buffer is full */
            auto co = s->running();
            waitx* w = this->create_waitx(co, (void*)p);
            if (w->buf != p) memcpy(w->buf, p, _blk_size);
            _wq.push_back(w);
            _m.unlock();

            if (co->s != s) co->s = s;
            co->waitx = (co::waitx_t*)w;

            if (_ms != (uint32)-1) s->add_timer(_ms);
            s->yield();

            if (!s->timeout()) co::free(w, w->len);
            co->waitx = 0;
        }
    }
}

Pipe::Pipe(uint32 buf_size, uint32 blk_size, uint32 ms) {
    _p = (uint32*) co::alloc(sizeof(PipeImpl) + 8);
    _p[0] = 1;
    new (_p + 2) PipeImpl(buf_size, blk_size, ms);
}

Pipe::~Pipe() {
    if (_p && atomic_dec(_p, mo_acq_rel) == 0) {
        ((PipeImpl*)(_p + 2))->~PipeImpl();
        co::free(_p, sizeof(PipeImpl) + 8);
    }
}

void Pipe::read(void* p) const {
    ((PipeImpl*)(_p + 2))->read(p);
}

void Pipe::write(const void* p) const {
    ((PipeImpl*)(_p + 2))->write(p);
}

} // xx

} // co
