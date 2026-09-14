#pragma once

#include "def.h"
#include "closure.h"
#include "sock.h"

namespace co {
namespace xx {

struct CoInit {
    CoInit();
    ~CoInit();
};

static CoInit g_co_init;

} // xx

void go(once_closure&& c);

inline void go(void (*f)()) {
    go(once_closure(f));
}

template<typename F, typename = std::enable_if_t<
    !std::is_convertible_v<std::decay_t<F>, void(*)()>>, typename ... A>
inline void go(F&& f, A&& ... a) {
    go(once_closure(std::forward<F>(f), std::forward<A>(a)...));
}

typedef void coro_t;
typedef void sched_t;

// get number of schedulers
int sched_num();

// get the current scheduler, NULL if called from non-scheduler thread
sched_t* sched();

// get the current coroutine
coro_t* coroutine();

// return id of the current scheduler, or -1 if called from non-scheduler thread
int sched_id();

// return id of the current coroutine, or -1 if called from non-coroutine
int coroutine_id();

// suspend the current coroutine 
void yield();

// resume a coroutine, @c is the result of co::coroutine()
void resume(coro_t* c);

// sleep for milliseconds in coroutine
void sleep(uint32 ms);

// check whether the current coroutine has timed out 
bool timeout();

// check if the memory @p points to is on the stack of the current coroutine 
bool on_stack(const void* p);

// add a timer for the current coroutine 
void add_timer(uint32 ms);

// add an IO event to the socket
void add_io_event(sock_t fd, ev_t ev);

// remove an IO event from the socket
void del_io_event(sock_t fd, ev_t ev);

// remove all IO events from the socket 
void del_io_event(sock_t fd);

// mutex lock for coroutines
struct mutex {
    mutex();
    ~mutex();

    mutex(mutex&& c) noexcept : _p(c._p) { c._p = 0; }

    // copy constructor, increment the reference count only
    mutex(const mutex& c);

    void operator=(const mutex&) = delete;
    void operator=(mutex&&) = delete;

    void lock() const;

    void unlock() const;

    bool try_lock() const;

    void* _p;
};

struct mutex_guard {
    explicit mutex_guard(const mutex& m) : _m(m) {
        _m.lock();
    }

    ~mutex_guard() {
        _m.unlock();
    }

    mutex_guard(const mutex_guard&) = delete;
    mutex_guard(mutex_guard&&) = delete;
    void operator=(const mutex_guard&) = delete;
    void operator=(mutex_guard&&) = delete;

    const co::mutex& _m;
};

// for communications between coroutines and/or threads
struct event {
    explicit event(bool manual_reset=false, bool signaled=false);
    ~event();

    event(event&& e) noexcept : _p(e._p) {
        e._p = 0;
    }

    // copy constructor, increment the reference count only
    event(const event& e);

    void operator=(const event&) = delete;
    void operator=(event&&) = delete;

    void wait() const;

    // return false if timedout
    bool wait(uint32 ms) const ;

    void notify_one() const;
    void notify_all() const;
    void reset() const;

    void* _p;
};

struct wait_group {
    explicit wait_group(uint32 n);

    wait_group() : wait_group(0) {}

    ~wait_group();

    wait_group(wait_group&& wg) noexcept : _p(wg._p) {
        wg._p = 0;
    }

    // copy constructor, increment the reference count only
    wait_group(const wait_group& wg);

    void operator=(const wait_group&) = delete;
    void operator=(wait_group&&) = delete;

    // increase the counter by n (1 by default)
    void add(uint32 n=1) const;

    // decrease the counter by 1, wake up all the waiters if it becomes 0
    void done() const;

    // blocks until the counter becomes 0
    void wait() const;

    void* _p;
};

// pool used in coroutines
struct pool {
    using create_cb_t = void* (*)();
    using destroy_cb_t = void (*)(void*);

    pool();
    ~pool();

    // @c    callback used to create an element
    //       eg.  []() { return (void*) new T; }
    // @d    callback used to destroy an element
    //       eg.  [](void* p) { delete (T*)p; }
    // @cap  max capacity of the pool per thread
    pool(create_cb_t c, destroy_cb_t d, uint32 cap=(uint32)-1);

    pool(pool&& p) : _p(p._p) { p._p = 0; }

    // copy constructor, increment the reference count only
    pool(const pool& p);

    void operator=(const pool&) = delete;
    void operator=(pool&&) = delete;

    // pop an element from the pool of the current thread 
    void* pop() const;

    // push an element to the pool of the current thread, ignored if e is NULL
    void push(void* e) const;

    void* _p;
};

// pop and hold a pointer from co::pool when constructed,
// and push it back when destructed
template<typename T>
struct pool_guard {
    explicit pool_guard(const pool& p) : _p(p) {
        _e = (T*) _p.pop();
    }

    explicit pool_guard(const pool* p) : pool_guard(*p) {}

    ~pool_guard() { _p.push(_e); }

    pool_guard(const pool_guard&) = delete;
    pool_guard(pool_guard&&) = delete;
    void operator=(const pool_guard&) = delete;
    void operator=(pool_guard&&) = delete;

    T* operator->() const noexcept { runtime_assert(_e); return _e; }
    T& operator*()  const noexcept { runtime_assert(_e); return *_e; }

    bool operator==(T* e) const noexcept { return _e == e; }
    bool operator!=(T* e) const noexcept { return _e != e; }
    explicit operator bool() const noexcept { return _e != nullptr; }

    T* get() const noexcept { return _e; }

    const pool& _p;
    T* _e;
};

} // co

using co::go;
