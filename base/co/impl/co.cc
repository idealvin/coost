#include "../co.h"
#include "scheduler.h"
#include <deque>

namespace co {

// coroutine Event implementation
class EventImpl {
  public:
    EventImpl() = default;
    ~EventImpl() = default;

    void wait();

    bool wait(unsigned int ms);

    void signal();

  private:
    ::Mutex _mtx;
    std::unordered_map<Coroutine*, timer_id_t> _co_wait;
};

void EventImpl::wait() {
    Coroutine* co = gSched->running();
    co->ev = ev_wait;
    if (co->s != gSched) co->s = gSched;
    {
        ::MutexGuard g(_mtx);
        _co_wait.insert(std::make_pair(co, null_timer_id));
    }

    gSched->yield();
    co->ev = 0;
}

bool EventImpl::wait(unsigned int ms) {
    Coroutine* co = gSched->running();
    co->ev = ev_wait;
    if (co->s != gSched) co->s = gSched;

    timer_id_t id = gSched->add_ev_timer(ms);
    {
        ::MutexGuard g(_mtx);
        _co_wait.insert(std::make_pair(co, id));
    }
    gSched->yield();

    if (gSched->timeout()) {
        ::MutexGuard g(_mtx);
        _co_wait.erase(co);
    }

    co->ev = 0;
    return !gSched->timeout();
}

void EventImpl::signal() {
    std::unordered_map<Coroutine*, timer_id_t> co_wait;
    {
        ::MutexGuard g(_mtx);
        _co_wait.swap(co_wait);
    }

    for (auto it = co_wait.begin(); it != co_wait.end(); ++it) {
        Coroutine* co = it->first;
        if (atomic_compare_swap(&co->ev, ev_wait, ev_ready) == ev_wait) {
            co->s->add_task(co, it->second);
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

// coroutine Mutex implementation
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

inline void MutexImpl::lock() {
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
        co->s->add_task(co);
    }
}

inline bool MutexImpl::try_lock() {
    ::MutexGuard g(_mtx);
    return _lock ? false : (_lock = true);
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

// coroutine Pool implementation
class PoolImpl {
  public:
    PoolImpl() : _pool(os::cpunum()) {}
    ~PoolImpl() = default;

    void* pop() {
        auto& v = _pool[gSched->id()];
        if (!v.empty()) {
            void* p = v.back();
            v.pop_back();
            return p;
        }
        return 0;
    }

    void push(void* p) {
        if (p) _pool[gSched->id()].push_back(p);
    }

    void clear(const std::function<void(void*)>& cb) {
        if (!gSched) return;
        auto& v = _pool[gSched->id()];
        if (cb) for (size_t i = 0; i < v.size(); ++i) cb(v[i]);
        v.clear();
    }

  private:
    std::vector<std::vector<void*>> _pool;
};

Pool::Pool() {
    _p = new PoolImpl;
}

Pool::~Pool() {
    delete (PoolImpl*) _p;
}

void* Pool::pop() {
    return ((PoolImpl*)_p)->pop();
}

void Pool::push(void* p) {
    ((PoolImpl*)_p)->push(p);
}

void Pool::clear(const std::function<void(void*)>& cb) {
    ((PoolImpl*)_p)->clear(cb);
}

} // co
