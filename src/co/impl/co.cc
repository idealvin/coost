#include "co/co.h"
#include "co/os.h"
#include <deque>
#include <unordered_set>

namespace co {

void go(Closure* cb) {
    sched_mgr()->next()->add_new_task(cb);
}

void sleep(uint32 ms) {
    gSched ? gSched->sleep(ms) : sleep::ms(ms);
}

void stop() {
    sched_mgr()->stop();
}

int max_sched_num() {
    static int kMaxSchedNum = os::cpunum();
    return kMaxSchedNum;
}

int sched_id() {
    return gSched ? gSched->id() : -1;
}

int coroutine_id() {
    return (gSched && gSched->running()) ? gSched->running()->id : -1;
}

class EventImpl {
  public:
    EventImpl() = default;
    ~EventImpl() = default;

    void wait();

    bool wait(unsigned int ms);

    void signal();

  private:
    ::Mutex _mtx;
    std::unordered_set<Coroutine*> _co_wait;
};

void EventImpl::wait() {
    CHECK(gSched) << "must be called in coroutine..";
    Coroutine* co = gSched->running();
    co->state = S_wait;
    if (co->s != gSched) co->s = gSched;
    assert(co->it == null_timer_id);
    {
        ::MutexGuard g(_mtx);
        _co_wait.insert(co);
    }

    gSched->yield();
    co->state = S_init;
}

bool EventImpl::wait(unsigned int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    Coroutine* co = gSched->running();
    co->state = S_wait;
    if (co->s != gSched) co->s = gSched;

    gSched->add_timer(ms);
    {
        ::MutexGuard g(_mtx);
        _co_wait.insert(co);
    }
    gSched->yield();

    if (gSched->timeout()) {
        ::MutexGuard g(_mtx);
        _co_wait.erase(co);
    }

    co->state = S_init;
    return !gSched->timeout();
}

void EventImpl::signal() {
    std::unordered_set<Coroutine*> co_wait;
    {
        ::MutexGuard g(_mtx);
        if (!_co_wait.empty()) _co_wait.swap(co_wait);
    }

    // Using atomic operation here, as check_timeout() in the Scheduler 
    // may also modify the state.
    for (auto it = co_wait.begin(); it != co_wait.end(); ++it) {
        Coroutine* co = *it;
        if (atomic_compare_swap(&co->state, S_wait, S_ready) == S_wait) {
            co->s->add_ready_task(co);
        }
    }
}

Event::Event() {
    _p = new EventImpl;
}

Event::~Event() {
    delete (EventImpl*) _p;
}

void Event::wait() {
    ((EventImpl*)_p)->wait();
}

bool Event::wait(unsigned int ms) {
    return ((EventImpl*)_p)->wait(ms);
}

void Event::signal() {
    ((EventImpl*)_p)->signal();
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
    CHECK(gSched) << "must be called in coroutine..";
    _mtx.lock();
    if (!_lock) {
        _lock = true;
        _mtx.unlock();
    } else {
        Coroutine* co = gSched->running();
        if (co->s != gSched) co->s = gSched;
        _co_wait.push_back(co);
        _mtx.unlock();
        gSched->yield();
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
        co->s->add_ready_task(co);
    }
}

Mutex::Mutex() {
    _p = new MutexImpl;
}

Mutex::~Mutex() {
    delete (MutexImpl*) _p;
}

void Mutex::lock() {
    ((MutexImpl*)_p)->lock();
}

void Mutex::unlock() {
    ((MutexImpl*)_p)->unlock();
}

bool Mutex::try_lock() {
    return ((MutexImpl*)_p)->try_lock();
}

class PoolImpl {
  public:
    typedef std::vector<void*> V;

    PoolImpl()
        : _pools(co::max_sched_num()), _maxcap((size_t)-1) {
    }

    // @ccb:  a create callback       []() { return (void*) new T; }
    // @dcb:  a destroy callback      [](void* p) { delete (T*)p; }
    // @cap:  max capacity for each pool
    PoolImpl(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap)
        : _pools(co::max_sched_num()), _maxcap(cap) {
        _ccb = std::move(ccb);
        _dcb = std::move(dcb);
    }

    ~PoolImpl() = default;

    void* pop() {
        CHECK(gSched) << "must be called in coroutine..";
        auto& v = _pools[gSched->id()];
        if (v == NULL) v = this->create_pool();

        if (!v->empty()) {
            void* p = v->back();
            v->pop_back();
            return p;
        } else {
            return _ccb ? _ccb() : 0;
        }
    }

    void push(void* p) {
        if (!p) return; // ignore null pointer

        CHECK(gSched) << "must be called in coroutine..";
        auto& v = _pools[gSched->id()];
        if (v == NULL) v = this->create_pool();

        if (!_dcb || v->size() < _maxcap) {
            v->push_back(p);
        } else {
            _dcb(p);
        }
    }

  private:
    // It is not safe to cleanup the pool from outside the Scheduler.
    // So we add a cleanup callback to the Scheduler. It will be called 
    // at the end of Scheduler::loop().
    V* create_pool() {
        V* v = new V();
        v->reserve(1024);
        gSched->add_cleanup_cb(std::bind(&PoolImpl::cleanup, v, _dcb));
        return v;
    }

    static void cleanup(V* v, const std::function<void(void*)>& dcb) {
        if (dcb) {
            for (size_t i = 0; i < v->size(); ++i) dcb((*v)[i]);
        }
        delete v;
    }

  private:
    std::vector<V*> _pools;
    std::function<void*()> _ccb;
    std::function<void(void*)> _dcb;
    size_t _maxcap;
};

Pool::Pool() {
    _p = new PoolImpl;
}

Pool::~Pool() {
    delete (PoolImpl*) _p;
}

Pool::Pool(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap) {
    _p = new PoolImpl(std::move(ccb), std::move(dcb), cap);
}

void* Pool::pop() {
    return ((PoolImpl*)_p)->pop();
}

void Pool::push(void* p) {
    ((PoolImpl*)_p)->push(p);
}

} // co
