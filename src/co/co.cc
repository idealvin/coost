#include "co/co.h"
#include "co/atomic.h"
#include "co/flag.h"
#include "co/mem.h"
#include "co/clist.h"
#include "co/os.h"
#include "sched.h"
#include "../thread.h"

#define SS(name, c, e) static const char* name[2] = { c, e };
SS(s_co_sched_num, "@i 协程调度器数量", "@i number of coroutine schedulers");
SS(s_co_stack_num, "@i 协程调度器的栈数量(必须是2的n次方)", "@i number of stacks per scheduler(must be power of 2)");
SS(s_co_stack_size, "@i 协程栈大小", "@i size of the coroutine stack");

DEF_uint32(co_sched_num, os::cpunum(), s_co_sched_num);
DEF_uint32(co_stack_num, 8, s_co_stack_num);
DEF_uint32(co_stack_size, 1024 * 1024, s_co_stack_size);


namespace co {

struct __cacheline_aligned Mod {
    Mod() : seed(co::rand()) {}
    ~Mod() = default;

    void add_task(void* c) {
        !scheds.empty() ? next_sched()->add_new_task(c) : tasks.push_back(c);
    }

    void start_scheds();

    uint32 seed;
    co::vector<Sched*> scheds;
    co::vector<void*> tasks;
    Sched* (*next_sched)();
};

static Mod* g_mod;

// as once_closure is trivially destructible, pass c._p is ok
void go(once_closure&& c) {
    static_assert(std::is_trivially_destructible_v<once_closure>, "");
    g_mod->add_task(c._p);
}

int sched_num() { return g_sched_num; }

sched_t* sched() { return (sched_t*) g_sched; }

coro_t* coroutine() {
    const auto s = g_sched;
    return s ? (coro_t*)s->running() : nullptr;
}

int sched_id() {
    const auto s = g_sched;
    return s ? s->id() : -1;
}

int coroutine_id() {
    const auto s = g_sched;
    return (s && s->running()) ? s->coroutine_id() : -1;
}

void yield() {
    const auto s = g_sched;
    s->yield();
}

void resume(coro_t* p) {
    const auto co = (Coroutine*)p;
    co->sched->add_ready_task(co);
}

void sleep(uint32 ms) {
    const auto s = g_sched;
    s ? s->sleep(ms) : time::sleep(ms);
}

bool timeout() {
    const auto s = g_sched;
    return s->timeout();
}

bool on_stack(const void* p) {
    const auto s = g_sched;
    return s->on_stack(p);
}

void add_timer(uint32 ms) {
    const auto s = g_sched;
    s->add_timer(ms);
}

void add_io_event(sock_t fd, ev_t ev) {
    const auto s = g_sched;
    s->add_io_event(fd, ev);
}

void del_io_event(sock_t fd, ev_t ev) {
    const auto s = g_sched;
    s->del_io_event(fd, ev);
}

void del_io_event(sock_t fd) {
    const auto s = g_sched;
    s->del_io_event(fd);
}

struct __cacheline_aligned mutex_impl {
    mutex_impl()
        : _m(), _wq(), _refn(1), _lock(0) {
    }

    ~mutex_impl() = default;

    void lock();
    void unlock();
    bool try_lock() {
        return atomic_bool_cas(&_lock, 0, 1, mo_acquire, mo_relaxed);
    }

    void ref() { atomic_inc(&_refn, mo_relaxed); }
    uint32 unref() { return atomic_dec(&_refn, mo_acq_rel); }

    _mutex _m;
    co::clist _wq;
    uint32 _refn;
    uint32 _lock;
};

void mutex_impl::lock() {
    if (this->try_lock()) return;

    const auto s = g_sched;
    if (s) { /* in coroutine */
        _m.lock();
        if (this->try_lock()) {
            _m.unlock();
            return;
        }

        _wq.push_back((clink*)s->running());
        _m.unlock();
        s->yield();

    } else { /* non-coroutine */
        _mutex_guard g(_m);
        if (this->try_lock()) return;

        const auto cv = _get_cv();
        cv->pred = false;

        alignas(Coroutine) char buf[sizeof(Coroutine)];
        Coroutine* const co = (Coroutine*) buf;
        co->sched = nullptr;
        co->wtx = (Waitx*) cv;
        _wq.push_back((clink*)co);

        // recheck cv->pred as spurious wakeups may happen
        do {
            cv->wait(_m.native_handle());
        } while (!cv->pred);
    }
}

void mutex_impl::unlock() {
    _m.lock();
    if (_wq.empty()) {
        atomic_store(&_lock, 0, mo_release);
        _m.unlock();
        return;
    }

    Coroutine* const co = (Coroutine*) _wq.pop_front();
    if (co->sched) {
        _m.unlock();
        co->sched->add_ready_task(co);
    } else {
        auto cv = (_cv*) co->wtx;
        cv->pred = true;
        _m.unlock();
        cv->notify_one();
    }
}

mutex::mutex() {
    _p = co::alloc(sizeof(mutex_impl), co::cache_line_size);
    runtime_assert(_p);
    new (_p) mutex_impl();
}

mutex::mutex(const mutex& m) : _p(m._p) {
    if (_p) static_cast<mutex_impl*>(_p)->ref();
}

mutex::~mutex() {
    const auto p = (mutex_impl*)_p;
    if (p && p->unref() == 0) {
        p->~mutex_impl();
        co::free(_p, sizeof(mutex_impl));
        _p = nullptr;
    }
}

void mutex::lock() const {
    static_cast<mutex_impl*>(_p)->lock();
}

void mutex::unlock() const {
    static_cast<mutex_impl*>(_p)->unlock();
}

bool mutex::try_lock() const {
    return static_cast<mutex_impl*>(_p)->try_lock();
}

struct __cacheline_aligned event_impl {
    event_impl(bool manual_reset, bool signaled)
        : _m(), _wq(), _refn(1),
          _state(!signaled ? 0 : 2), _manual_reset(manual_reset) {
    }

    ~event_impl() = default;

    bool _is_signaled() {
        return _manual_reset
            ? atomic_load(&_state, mo_acquire) == 2
            : atomic_bool_cas(&_state, 2, 0, mo_acquire, mo_relaxed);
    }

    void reset() {
        atomic_store(&_state, 0, mo_relaxed);
    }

    void wait();
    bool wait(uint32 ms);
    void notify_one();
    void notify_all();

    void ref() { atomic_inc(&_refn, mo_relaxed); }
    uint32 unref() { return atomic_dec(&_refn, mo_acq_rel); }

    _mutex _m;
    co::clist _wq;
    uint32 _refn;
    uint8 _state; // 0: unsignaled, 1: notifying, 2: signaled
    const bool _manual_reset;
};

void event_impl::wait() {
    if (_is_signaled()) return;

    const auto s = g_sched;
    Coroutine* co = s ? s->running() : nullptr;

    if (co) { /* in coroutine */
        _m.lock();
        if (_is_signaled()) {
            _m.unlock();
            return;
        }

        static_assert(offsetof(Coroutine, wtx) == offsetof(Waitx, co));
        auto w = (Waitx*) co;
        w->co = co; // co->wtx = co
        _wq.push_back((clink*)w);
        _m.unlock();
        s->yield();
        co->wtx = nullptr;

    } else { /* non-coroutine */
        _mutex_guard g(_m);
        if (_is_signaled()) return;

        const auto cv = _get_cv();
        cv->pred = false;
        Waitx w;
        w.co = nullptr;
        w.ud = (void*)cv;
        _wq.push_back((clink*)&w);

        do {
            cv->wait(_m.native_handle());
        } while (!cv->pred);
    }
}

bool event_impl::wait(uint32 ms) {
    if (_is_signaled()) return true;
    if (ms == 0) return false;

    const auto s = g_sched;
    Coroutine* co = s ? s->running() : nullptr;

    if (co) { /* in coroutine */
        _m.lock();
        if (_is_signaled()) {
            _m.unlock();
            return true;
        }

        Waitx* w = (Waitx*) co::alloc(sizeof(Waitx), co::cache_line_size);
        runtime_assert(w);
        w->co = co;
        w->state = 0; // waiting
        co->wtx = w;
        _wq.push_back((clink*)w);
        _m.unlock();

        s->add_timer(ms);
        s->yield();

        if (s->timeout()) {
            _mutex_guard g(_m);
            if (((clink*)w)->prev) _wq.erase((clink*)w); // w is still in the list
        }

        co::free(w, sizeof(Waitx));
        co->wtx = nullptr;
        return !s->timeout();
       
    } else { /* non-coroutine */
        _mutex_guard g(_m);
        if (_is_signaled()) return true;

        const auto cv = _get_cv();
        cv->pred = false;
        Waitx w;
        w.co = nullptr;
        w.ud = (void*)cv;
        _wq.push_back((clink*)&w);

        uint32 t = 0;
        co::timer timer;

        while (true) {
            bool r = cv->wait(_m.native_handle(), ms - t);
            if (cv->pred) return true; // signaled
            if (r) {
                t = (uint32) timer.ms();
                if (t < ms) continue; // spurious wakeup, continue waiting
            }
            _wq.erase((clink*)&w);
            return false; // timedout
        }
    }
}

void event_impl::notify_one() {
    if (atomic_bool_cas(&_state, 0, 1, mo_relaxed, mo_relaxed)) {
        _mutex_guard g(_m);
        while (!_wq.empty()) {
            Waitx* w = (Waitx*) _wq.pop_front();
            const auto co = w->co;
            if (co) { /* wait in coroutine */
                // co == w, wait without timeout
                // co != w, wait with a timeout
                if ((void*)co == w ||
                    atomic_bool_cas(&w->state, 0, 1, mo_relaxed, mo_relaxed)) {
                    co->sched->add_ready_task(co);
                    atomic_store(
                        &_state,
                        _manual_reset ? 2 : 0,
                        _manual_reset ? mo_release : mo_relaxed
                    );
                    return;
                }

                // timedout, if w->prev is NULL, w is not in the list
                ((clink*)w)->prev = nullptr;

            } else {
                _cv* cv = (_cv*) w->ud;
                cv->pred = true;
                cv->notify_one();
                atomic_store(
                    &_state,
                    _manual_reset ? 2 : 0,
                    _manual_reset ? mo_release : mo_relaxed
                );
                return;
            }
        }

        // no waiter notified
        atomic_store(&_state, 2, mo_release);
    }
}

void event_impl::notify_all() {
    if (atomic_bool_cas(&_state, 0, 1, mo_relaxed, mo_relaxed)) {
        bool notified = false;
        _mutex_guard g(_m);
        for (clink* c = _wq.front(); c;) {
            // get next node here as w may be freed after add_ready_task(co)
            Waitx* const w = (Waitx*)c;
            c = c->next;

            const auto co = w->co;
            if (co) {
                if ((void*)co == w ||
                    atomic_bool_cas(&w->state, 0, 1, mo_relaxed, mo_relaxed)) {
                    co->sched->add_ready_task(co);
                    if (!notified) notified = true;
                } else { /* timedout */
                    ((clink*)w)->prev = nullptr;
                }
            } else {
                _cv* cv = (_cv*) w->ud;
                cv->pred = true;
                cv->notify_one();
                if (!notified) notified = true;
            }
        }
        if (!_wq.empty()) _wq.clear();

        const bool x = (_manual_reset || !notified);
        atomic_store(&_state, x ? 2 : 0, x ? mo_release : mo_relaxed);
    }
}

event::event(bool manual_reset, bool signaled) {
    _p = co::alloc(sizeof(event_impl), co::cache_line_size);
    runtime_assert(_p);
    new (_p) event_impl(manual_reset, signaled);
}

event::event(const event& e) : _p(e._p) {
    if (_p) static_cast<event_impl*>(_p)->ref();
}

event::~event() {
    const auto p = (event_impl*)_p;
    if (p && p->unref() == 0) {
        p->~event_impl();
        co::free(_p, sizeof(event_impl));
        _p = 0;
    }
}

void event::wait() const {
    static_cast<event_impl*>(_p)->wait();
}

bool event::wait(uint32 ms) const {
    return static_cast<event_impl*>(_p)->wait(ms);
}

void event::notify_one() const {
    static_cast<event_impl*>(_p)->notify_one();
}

void event::notify_all() const {
    static_cast<event_impl*>(_p)->notify_all();
}

void event::reset() const {
    static_cast<event_impl*>(_p)->reset();
}


struct __cacheline_aligned wait_group_impl {
    wait_group_impl(uint32 n)
        : _m(), _wq(), _refn(1), _n(n) {
    }

    ~wait_group_impl() = default;

    void add(uint32 n) {
        atomic_add(&_n, n, mo_relaxed);
    }

    void done();
    void wait();

    void ref() { atomic_inc(&_refn, mo_relaxed); }
    uint32 unref() { return atomic_dec(&_refn, mo_acq_rel); }

    _mutex _m;
    clist _wq;
    uint32 _refn;
    uint32 _n;
};

void wait_group_impl::done() {
    const uint32 n = atomic_dec(&_n, mo_acq_rel);
    runtime_assert(n != (uint32)-1);

    if (n == 0) {
        _mutex_guard g(_m);
        if (!_wq.empty()) {
            _wq.for_each([](clink* c) {
                Coroutine* const co = (Coroutine*)c;
                if (co->sched) {
                    co->sched->add_ready_task(co);
                } else {
                    _cv* const cv = (_cv*) co->wtx;
                    cv->pred = true;
                    cv->notify_one();
                }
            });
            _wq.clear();
        }
    }
}

void wait_group_impl::wait() {
    if (atomic_load(&_n, mo_acquire) == 0) return;

    const auto s = g_sched;
    if (s) {
        _m.lock();
        if (atomic_load(&_n, mo_acquire) == 0) {
            _m.unlock();
            return;
        }

        _wq.push_back((clink*)s->running());
        _m.unlock();
        s->yield();

    } else {
        _mutex_guard g(_m);
        if (atomic_load(&_n, mo_acquire) == 0) return;

        const auto cv = _get_cv();
        cv->pred = false;

        alignas(Coroutine) char buf[sizeof(Coroutine)];
        Coroutine* const co = (Coroutine*) buf;
        co->sched = nullptr;
        co->wtx = (Waitx*) cv;
        _wq.push_back((clink*)co);

        do {
            cv->wait(_m.native_handle());
        } while (!cv->pred);
    }
}

wait_group::wait_group(uint32 n) {
    _p = co::alloc(sizeof(wait_group_impl), co::cache_line_size);
    runtime_assert(_p);
    new (_p) wait_group_impl(n);
}

wait_group::wait_group(const wait_group& wg) : _p(wg._p) {
    if (_p) static_cast<wait_group_impl*>(_p)->ref();
}

wait_group::~wait_group() {
    const auto p = (wait_group_impl*)_p;
    if (p && p->unref() == 0) {
        p->~wait_group_impl();
        co::free(_p, sizeof(wait_group_impl));
        _p = 0;
    }
}

void wait_group::add(uint32 n) const {
    static_cast<wait_group_impl*>(_p)->add(n);
}

void wait_group::done() const {
    static_cast<wait_group_impl*>(_p)->done();
}

void wait_group::wait() const {
    static_cast<wait_group_impl*>(_p)->wait();
}


struct __cacheline_aligned pool_impl {
    using V = co::vector<void*>;
    using create_cb_t = void* (*)();
    using destroy_cb_t = void (*)(void*);

    pool_impl()
        : _cap((uint32)-1), _refn(1), _c(nullptr), _d(nullptr) {
        _pools.resize(g_sched_num);
    }

    pool_impl(create_cb_t c, destroy_cb_t d, uint32 cap)
        : _cap(cap), _refn(1), _c(c), _d(d) {
        _pools.resize(g_sched_num);
    }

    ~pool_impl() {
        this->clear();
    }

    void* pop();
    void push(void* p);
    void clear();

    void ref() { atomic_inc(&_refn, mo_relaxed); }
    uint32 unref() { return atomic_dec(&_refn, mo_acq_rel); }

    co::vector<V> _pools;
    uint32 _cap;
    uint32 _refn;
    create_cb_t _c;
    destroy_cb_t _d;
};

inline void* pool_impl::pop() {
    auto s = g_sched;
    runtime_assert(s, "must be called in coroutine");

    auto& v = _pools[s->id()];
    if (!v.empty()) {
        const auto p = v.back();
        v.pop_back();
        return p;
    }
    return _c ? _c() : nullptr;
}

inline void pool_impl::push(void* p) {
    auto s = g_sched;
    runtime_assert(s, "must be called in coroutine");

    if (p) {
        auto& v = _pools[s->id()];
        ((uint32)v.size() < _cap || !_d) ? v.push_back(p) : _d(p);
    }
}

void pool_impl::clear() {
    for (auto& v : _pools) {
        if (_d) {
            for (auto& x : v) _d(x);
        }
        V().swap(v);
    }
}

pool::pool() {
    _p = co::alloc(sizeof(pool_impl), co::cache_line_size);
    runtime_assert(_p);
    new (_p) pool_impl();
}

pool::pool(create_cb_t c, destroy_cb_t d, uint32 cap) {
    _p = co::alloc(sizeof(pool_impl), co::cache_line_size);
    runtime_assert(_p);
    new (_p) pool_impl(c, d, cap);
}

pool::pool(const pool& p) : _p(p._p) {
    if (_p) static_cast<pool_impl*>(_p)->ref();
}

pool::~pool() {
    const auto p = (pool_impl*)_p;
    if (p && p->unref() == 0) {
        p->~pool_impl();
        co::free(_p, sizeof(pool_impl));
        _p = 0;
    }
}

void* pool::pop() const {
    return static_cast<pool_impl*>(_p)->pop();
}

void pool::push(void* p) const {
    static_cast<pool_impl*>(_p)->push(p);
}

void Mod::start_scheds() {
    auto& n = g_sched_num;
    if (n != 1) {
        if ((n & (n - 1)) == 0) {
            next_sched = []() {
                const uint32 i = co::rand(g_mod->seed) & (g_sched_num - 1);
                return g_mod->scheds[i];
            };
        } else {
            next_sched = []() {
                const uint32 i = co::rand(g_mod->seed) % g_sched_num;
                return g_mod->scheds[i];
            };
        }
    } else {
        next_sched = []() {
            return g_mod->scheds[0];
        };
    }

    scheds.reserve(n);
    for (uint32 i = 0; i < n; ++i) {
        Sched* s = co::_make_static<Sched>(i);
        s->start();
        scheds.push_back(s);
    }

    if (!tasks.empty()) {
        for (size_t i = 0; i < tasks.size(); ++i) {
            next_sched()->add_new_task(tasks[i]);
        }
        tasks.clear();
        co::vector<void*>().swap(tasks);
    }
}

namespace xx {

static int g_nifty_counter;

CoInit::CoInit() {
    const int n = ++g_nifty_counter;
    if (n == 1) {
        g_sched_num = os::cpunum();
        g_mod = co::_make_static<Mod>();
        g_all_scheds = &g_mod->scheds;
    }
    if (n == 2) {
        flag::run_before_parse([]() {
            flag::unhide("co_sched_num");
            flag::unhide("co_stack_num");
            flag::unhide("co_stack_size");
        });
        flag::run_after_parse([]() {
            const uint32 ncpu = os::cpunum();
            auto& n = FLG_co_sched_num;
            auto& m = FLG_co_stack_num;
            auto& s = FLG_co_stack_size;
            if (n == 0 || n > ncpu) n = ncpu;
            if (m == 0 || (m & (m - 1)) != 0) m = 8;
            if (s == 0) s = 1024 * 1024;
            g_sched_num = n;
            g_stack_num = m;
            g_stack_size = s;
            g_mod->start_scheds();
        });
    }
}

CoInit::~CoInit() {}

} // xx
} // co
