#include "scheduler.h"
#include <new>
#include <deque>
#include <unordered_set>

namespace co {

class EventImpl {
  public:
    EventImpl() : _counter(0), _signaled(false), _has_cond(false) {}
    ~EventImpl() { co::xx::cond_destroy(&_cond); }

    bool wait(uint32 ms);

    void signal();

  private:
    ::Mutex _mtx;
    co::xx::cond_t _cond;
    std::unordered_set<Coroutine*> _co_wait;
    std::unordered_set<Coroutine*> _co_swap;
    uint32 _counter;
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
            if (_signaled) { if (_counter == 0) _signaled = false; return true; }
            co->state = st_wait;
            _co_wait.insert(co);
        }

        if (ms != (uint32)-1) s->add_timer(ms);
        s->yield();
        if (s->timeout()) {
            ::MutexGuard g(_mtx);
            _co_wait.erase(co);
        }

        co->state = st_init;
        return !s->timeout();

    } else { /* not in coroutine */
        ::MutexGuard g(_mtx);
        if (!_signaled) {
            ++_counter;
            if (!_has_cond) { co::xx::cond_init(&_cond); _has_cond = true; }
            bool r = true;
            if (ms == (uint32)-1) {
                co::xx::cond_wait(&_cond, _mtx.mutex());
            } else {
                r = co::xx::cond_wait(&_cond, _mtx.mutex(), ms);
            }
            --_counter;
            if (!r) return false;
            assert(_signaled);
        }
        if (_counter == 0) _signaled = false;
        return true;
    }
}

void EventImpl::signal() {
    {
        ::MutexGuard g(_mtx);
        if (!_co_wait.empty()) _co_wait.swap(_co_swap);
        if (!_signaled) {
            _signaled = true;
            if (_counter > 0) {
                if (!_has_cond) { co::xx::cond_init(&_cond); _has_cond = true; }
                co::xx::cond_notify(&_cond);
            }
        }
        if (_co_swap.empty()) return;
    }

    // Using atomic operation here, as check_timeout() in the Scheduler 
    // may also modify the state.
    for (auto it = _co_swap.begin(); it != _co_swap.end(); ++it) {
        Coroutine* co = *it;
        if (atomic_compare_swap(&co->state, st_wait, st_ready) == st_wait) {
            ((SchedulerImpl*)co->s)->add_ready_task(co);
        }
    }
    _co_swap.clear();
}

// memory: |4(refn)|4|EventImpl|
Event::Event() {
    _p = (uint32*) malloc(sizeof(EventImpl) + 8);
    _p[0] = 1;
    new (_p + 2) EventImpl;
}

Event::~Event() {
    if (_p && atomic_dec(_p) == 0) {
        ((EventImpl*)(_p + 2))->~EventImpl();
        free(_p);
    }
}

bool Event::wait(uint32 ms) const {
    return ((EventImpl*)(_p + 2))->wait(ms);
}

void Event::signal() const {
    ((EventImpl*)(_p + 2))->signal();
}

// memory: |4(refn)|4(counter)|EventImpl|
WaitGroup::WaitGroup() {
    _p = (uint32*) malloc(sizeof(EventImpl) + 8);
    _p[0] = 1; // refn
    _p[1] = 0; // counter
    new (_p + 2) EventImpl;
}

WaitGroup::~WaitGroup() {
    if (_p && atomic_dec(_p) == 0) {
        ((EventImpl*)(_p + 2))->~EventImpl();
        free(_p);
    }
}

void WaitGroup::add(uint32 n) const {
    atomic_add(_p + 1, n);
}

void WaitGroup::done() const {
    CHECK_GT(*(_p + 1), 0);
    if (atomic_dec(_p + 1) == 0) ((EventImpl*)(_p + 2))->signal();
}

void WaitGroup::wait() const {
    ((EventImpl*)(_p + 2))->wait((uint32)-1);
}

class MutexImpl {
  public:
    MutexImpl() : _lock(false) {}
    ~MutexImpl() = default;

    void lock();

    void unlock();

    bool try_lock();

  private:
    ::Mutex _mtx;
    std::deque<Coroutine*> _co_wait;
    bool _lock;
};

inline bool MutexImpl::try_lock() {
    ::MutexGuard g(_mtx);
    return _lock ? false : (_lock = true);
}

inline void MutexImpl::lock() {
    auto s = gSched;
    CHECK(s) << "must be called in coroutine..";
    _mtx.lock();
    if (!_lock) {
        _lock = true;
        _mtx.unlock();
    } else {
        Coroutine* co = s->running();
        if (co->s != s) co->s = s;
        _co_wait.push_back(co);
        _mtx.unlock();
        s->yield();
    }
}

inline void MutexImpl::unlock() {
    _mtx.lock();
    if (_co_wait.empty()) {
        _lock = false;
        _mtx.unlock();
    } else {
        Coroutine* co = _co_wait.front();
        _co_wait.pop_front();
        _mtx.unlock();
        ((SchedulerImpl*)co->s)->add_ready_task(co);
    }
}

// memory: |4(refn)|4|MutexImpl|
Mutex::Mutex() {
    _p = (uint32*) malloc(sizeof(MutexImpl) + 8);
    _p[0] = 1; // refn
    new (_p + 2) MutexImpl;
}

Mutex::~Mutex() {
    if (_p && atomic_dec(_p) == 0) {
        ((MutexImpl*)(_p + 2))->~MutexImpl();
        free(_p);
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
    typedef std::vector<void*> V;

    PoolImpl()
        : _pools(co::scheduler_num()), _maxcap((size_t)-1) {
    }

    PoolImpl(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap)
        : _pools(co::scheduler_num()), _maxcap(cap), 
          _ccb(std::move(ccb)), _dcb(std::move(dcb)) {
    }

    ~PoolImpl() { this->clear(); }

    void* pop();

    void push(void* p);

    void clear();

    size_t size() const;

  private:
    std::vector<V*> _pools;
    std::function<void*()> _ccb;
    std::function<void(void*)> _dcb;
    size_t _maxcap;

    V* new_pool() { V* v = new V(); v->reserve(1024); return v; }
};

inline void* PoolImpl::pop() {
    CHECK(gSched) << "must be called in coroutine..";
    auto& v = _pools[gSched->id()];
    if (v == NULL) v = this->new_pool();
    if (!v->empty()) {
        void* p = v->back();
        v->pop_back();
        return p;
    } else {
        return _ccb ? _ccb() : 0;
    }
}

inline void PoolImpl::push(void* p) {
    if (!p) return; // ignore null pointer
    CHECK(gSched) << "must be called in coroutine..";
    auto& v = _pools[gSched->id()];
    if (v == NULL) v = this->new_pool();
    if (v->size() < _maxcap || !_dcb) {
        v->push_back(p);
    } else {
        _dcb(p);
    }
}

// Create n coroutines to clear all the pools, n is number of schedulers.
// clear() blocks untils all the coroutines are done.
void PoolImpl::clear() {
    auto& scheds = co::all_schedulers();
    WaitGroup wg;
    wg.add((uint32)scheds.size());

    for (auto& s : scheds) {
        s->go([this, wg]() {
            auto& v = this->_pools[gSched->id()];
            if (v != NULL) {
                if (this->_dcb) for (auto& e : *v) this->_dcb(e);
                delete v; v = NULL;
            }
            wg.done();
        });
    }

    wg.wait();
}

inline size_t PoolImpl::size() const {
    CHECK(gSched) << "must be called in coroutine..";
    auto& v = _pools[gSched->id()];
    return v ? v->size() : 0;
}

// memory: |4(refn)|4|PoolImpl|
Pool::Pool() {
    _p = (uint32*) malloc(sizeof(PoolImpl) + 8);
    _p[0] = 1;
    new (_p + 2) PoolImpl;
}

Pool::~Pool() {
    if (_p && atomic_dec(_p) == 0) {
        ((PoolImpl*)(_p + 2))->~PoolImpl();
        free(_p);
    }
}

Pool::Pool(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap) {
    _p = (uint32*) malloc(sizeof(PoolImpl) + 8);
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

} // co
