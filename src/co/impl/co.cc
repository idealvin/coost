#include "co/co.h"
#include "co/os.h"
#include "scheduler.h"
#include <deque>

namespace co {

void go(Closure* cb) {
    sched_mgr().next()->add_task(cb);
}

void sleep(uint32 ms) {
    gSched ? gSched->sleep(ms) : sleep::ms(ms);
}

void stop() {
    sched_mgr().stop();
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
    std::unordered_map<Coroutine*, timer_id_t> _co_wait;
};

void EventImpl::wait() {
    CHECK(gSched) << "must be called in coroutine..";
    Coroutine* co = gSched->running();
    co->state = S_wait;
    if (co->s != gSched) co->s = gSched;
    {
        ::MutexGuard g(_mtx);
        _co_wait.insert(std::make_pair(co, null_timer_id));
    }

    gSched->yield();
    co->state = S_init;
}

bool EventImpl::wait(unsigned int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    Coroutine* co = gSched->running();
    co->state = S_wait;
    if (co->s != gSched) co->s = gSched;

    timer_id_t id = gSched->add_timer(ms);
    {
        ::MutexGuard g(_mtx);
        _co_wait.insert(std::make_pair(co, id));
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
    std::unordered_map<Coroutine*, timer_id_t> co_wait;
    {
        ::MutexGuard g(_mtx);
        if (!_co_wait.empty()) _co_wait.swap(co_wait);
    }

    for (auto it = co_wait.begin(); it != co_wait.end(); ++it) {
        Coroutine* co = it->first;
        if (atomic_compare_swap(&co->state, S_wait, S_ready) == S_wait) {
            if (it->second != null_timer_id) {
                co->s->add_task(co, it->second);
            } else {
                co->s->add_task(co);
            }
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
        co->s->add_task(co);
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
    PoolImpl()
        : _pool(co::max_sched_num()), _maxcap((size_t)-1) {
    }

    ~PoolImpl();

    void set_create_cb(std::function<void*()>&& cb) {
        _create_cb = std::move(cb);
    }

    void set_destroy_cb(std::function<void(void*)>&& cb) {
        _destroy_cb = std::move(cb);
    }

    void set_max_capacity(size_t cap) {
        _maxcap = cap;
    }

    void* pop() {
        CHECK(gSched) << "must be called in coroutine..";
        auto& v = _pool[gSched->id()];

        if (!v.empty()) {
            void* p = v.back();
            v.pop_back();
            return p;
        } else {
            return _create_cb ? _create_cb() : 0;
        }
    }

    void push(void* p) {
        if (!p) return; // ignore null pointer

        CHECK(gSched) << "must be called in coroutine..";
        auto& v = _pool[gSched->id()];

        if (!_destroy_cb || v.size() < _maxcap) { 
            v.push_back(p);
        } else {
            _destroy_cb(p);
        }
    }

  private:
    std::vector<std::vector<void*>> _pool;
    std::function<void*()> _create_cb;
    std::function<void(void*)> _destroy_cb;
    size_t _maxcap;
};

PoolImpl::~PoolImpl() {
    if (!_destroy_cb) return;

    for (size_t i = 0; i < _pool.size(); ++i) {
        auto& v = _pool[i];
        for (size_t k = 0; k < v.size(); ++k) {
            _destroy_cb(v[k]);
        }
        v.clear();
    }
}

Pool::Pool() {
    _p = new PoolImpl;
}

Pool::~Pool() {
    delete (PoolImpl*) _p;
}

void Pool::set_create_cb(std::function<void*()>&& cb) {
    ((PoolImpl*)_p)->set_create_cb(std::move(cb));
}

void Pool::set_destroy_cb(std::function<void(void*)>&& cb) {
    ((PoolImpl*)_p)->set_destroy_cb(std::move(cb));
}

void Pool::set_max_capacity(size_t cap) {
    ((PoolImpl*)_p)->set_max_capacity(cap);
}

void* Pool::pop() {
    return ((PoolImpl*)_p)->pop();
}

void Pool::push(void* p) {
    ((PoolImpl*)_p)->push(p);
}

} // co
