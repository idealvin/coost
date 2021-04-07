#pragma once

#include "os.h"
#include "co/sock.h"
#include "co/hook.h"
#include "co/epoll.h"
#include "co/scheduler.h"

#include <string.h>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>

namespace co {

/**
 * add a task, which will run as a coroutine 
 *   - new_closure() can be used to create a Closure, and the user needn't delete the 
 *     Closure manually, as it will delete itself after Closure::run() is called.
 *   - As the Closure is an abstract base class, the user is free to implement his or 
 *     hers own subtype of Closure. See details in co/closure.h.
 * 
 * @param cb  a pointer to a Closure by new_closure(), or an user-defined Closure
 */
inline void go(Closure* cb) {
    xx::scheduler_manager()->next_scheduler()->add_new_task(cb);
}

/**
 * add a task, which will run as a coroutine 
 * 
 * @param f  a pointer to a function: void xxx()
 */
inline void go(void (*f)()) {
    go(new_closure(f));
}

/**
 * add a task, which will run as a coroutine 
 * 
 * @param f  a pointer to a function with a parameter: void xxx(void*)
 * @param p  a pointer as the parameter of f
 */
inline void go(void (*f)(void*), void* p) {
    go(new_closure(f, p));
}

/**
 * add a task, which will run as a coroutine 
 * 
 * @tparam T  type of a class
 * @param f   a pointer to a method of class T:  void T::xxx()
 * @param o   a pointer to an object of class T
 */
template<typename T>
inline void go(void (T::*f)(), T* o) {
    go(new_closure(f, o));
}

/**
 * add a task, which will run as a coroutine 
 * 
 * @tparam T  type of a class
 * @param f   a pointer to a method of class T:  void T::xxx(void*)
 * @param o   a pointer to an object of class T
 * @param p   a pointer as the parameter of f
 */
template<typename T>
inline void go(void (T::*f)(void*), T* o, void* p) {
    go(new_closure(f, o, p));
}

/**
 * add a task, which will run as a coroutine 
 *   - It is a little expensive to create an object of std::function, try to 
 *     avoid it if the user cares about the performance. 
 * 
 * @param f  a reference of an object of std::function<void()>
 */
inline void go(const std::function<void()>& f) {
    go(new_closure(f));
}

/**
 * add a task, which will run as a coroutine 
 *   - It is a little expensive to create an object of std::function, try to 
 *     avoid it if the user cares about the performance. 
 * 
 * @param f  a rvalue reference of an object of std::function<void()>
 */
inline void go(std::function<void()>&& f) {
    go(new_closure(std::move(f)));
}

/**
 * get the current scheduler 
 *   
 * @return  a pointer to the current scheduler, or NULL if the current thread 
 *          is not a scheduler thread.
 */
inline xx::Scheduler* scheduler() {
    return xx::gSched;
}

/**
 * get all schedulers 
 *   
 * @return  a reference of an array, which stores pointers to all the Schedulers
 */
inline const std::vector<xx::Scheduler*>& all_schedulers() {
    return xx::scheduler_manager()->all_schedulers();
}

/**
 * get max number of schedulers 
 *   - scheduler id is from 0 to max_sched_num - 1. 
 * 
 * @return  a positive value, it is equal to the number of CPU cores.
 */
int max_sched_num();

/**
 * get id of the current scheduler 
 *   - It is EXPECTED to be called in a coroutine. 
 * 
 * @return  non-negative id of the current scheduler, or -1 if the current thread 
 *          is not a scheduler thread.
 */
inline int scheduler_id() {
    scheduler() ? scheduler()->id() : -1;
}

/**
 * get id of the current coroutine 
 *   - Coroutines in different Schedulers may have the same id.
 * 
 * @return  non-negative id of the current coroutine, 
 *          or -1 if it is called not in a coroutine.
 */
inline int coroutine_id() {
    return (scheduler() && scheduler()->running()) ? scheduler()->running()->id : -1;
}

/**
 * sleep for milliseconds 
 *   - It is EXPECTED to be called in a coroutine. 
 * 
 * @param ms  time in milliseconds
 */
inline void sleep(unsigned int ms) {
    scheduler() ? scheduler()->sleep(ms) : sleep::ms(ms);
}

/**
 * stop all coroutine schedulers 
 *   - It is safe to call stop() from anywhere. 
 */
inline void stop() {
    xx::scheduler_manager()->stop_all_schedulers();
}

// co::Event is for communications between coroutines.
// It's similar to SyncEvent for threads.
class Event {
  public:
    Event();
    ~Event();

    Event(Event&& e) : _p(e._p) { e._p = 0; }

    Event(const Event&) = delete;
    void operator=(const Event&) = delete;

    // MUST be called in coroutine
    void wait();

    // return false if timeout
    // MUST be called in coroutine
    bool wait(unsigned int ms);

    // wakeup all waiting coroutines
    // can be called from anywhere
    void signal();

  private:
    void* _p;
};

// co::Mutex is a mutex lock for coroutines.
// It's similar to Mutex for threads.
class Mutex {
  public:
    Mutex();
    ~Mutex();

    Mutex(Mutex&& m) : _p(m._p) { m._p = 0; }

    Mutex(const Mutex&) = delete;
    void operator=(const Mutex&) = delete;

    // MUST be called in coroutine
    void lock();

    // can be called from anywhere
    void unlock();

    // can be called from anywhere
    bool try_lock();

  private:
    void* _p;
};

class MutexGuard {
  public:
    explicit MutexGuard(co::Mutex& lock) : _lock(lock) {
        _lock.lock();
    }

    explicit MutexGuard(co::Mutex* lock) : _lock(*lock) {
        _lock.lock();
    }

    MutexGuard(const MutexGuard&) = delete;
    void operator=(const MutexGuard&) = delete;

    ~MutexGuard() {
        _lock.unlock();
    }

  private:
    co::Mutex& _lock;
};

// co::Pool is a general pool for coroutines.
// It stores void* pointers internally.
// It is coroutine-safe. Each thread has its own pool.
class Pool {
  public:
    Pool();
    ~Pool();

    // @ccb:  a create callback       []() { return (void*) new T; }
    //   when pop from an empty pool, this callback is used to create an element
    //
    // @dcb:  a destroy callback      [](void* p) { delete (T*)p; }
    //   this callback is used to destroy an element when needed
    //
    // @cap:  max capacity of the pool for each thread
    //   this argument is ignored if the destory callback is not set
    Pool(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap=(size_t)-1);

    Pool(Pool&& p) : _p(p._p) { p._p = 0; }

    Pool(const Pool&) = delete;
    void operator=(const Pool&) = delete;

    // pop an element from the pool
    // return NULL if the pool is empty and the create callback is not set
    // MUST be called in coroutine
    void* pop();

    // push an element to the pool
    // nothing is done if p is NULL
    // MUST be called in coroutine
    void push(void* p);

  private:
    void* _p;
};

// pop an element from co::Pool in constructor, and push it back in destructor.
// The element is stored internally as a pointer of type T*.
// As owner of the pointer, its behavior is similar to std::unique_ptr.
template<typename T>
class PoolGuard {
  public:
    explicit PoolGuard(Pool& pool) : _pool(pool) {
        _p = (T*) _pool.pop();
    }

    explicit PoolGuard(Pool* pool) : _pool(*pool) {
        _p = (T*) _pool.pop();
    }

    PoolGuard(const PoolGuard&) = delete;
    void operator=(const PoolGuard&) = delete;

    ~PoolGuard() {
        _pool.push(_p);
    }

    T* get() const {
        return _p;
    }

    void reset(T* p = 0) {
        if (_p != p) { delete _p; _p = p; }
    }

    void operator=(T* p) {
        this->reset(p);
    }

    T* operator->() const {
        return _p;
    }

    bool operator==(T* p) const {
        return _p == p;
    }

    bool operator!=(T* p) const {
        return _p != p;
    }

  private:
    Pool& _pool;
    T* _p;
};

#ifdef _WIN32
struct PerIoInfo {
    PerIoInfo(const void* data, int size, void* c)
        : n(0), flags(0), co(c), s(data ? 0 : (char*)malloc(size)) {
        memset(&ol, 0, sizeof(ol));
        buf.buf = data ? (char*)data : s;
        buf.len = size;
    }

    ~PerIoInfo() {
        if (s) free(s);
    }

    void move(DWORD n) {
        buf.buf += n;
        buf.len -= (ULONG) n;
    }

    void resetol() {
        memset(&ol, 0, sizeof(ol));
    }

    WSAOVERLAPPED ol;
    DWORD n;            // bytes transfered
    DWORD flags;        // flags for WSARecv
    void* co;           // user data, pointer to a coroutine
    char* s;            // dynamic allocated buffer
    WSABUF buf;
};

class IoEvent {
  public:
    // We don't care what kind of event it is on Windows.
    IoEvent(sock_t fd)
        : _fd(fd), _has_timer(false) {
        scheduler()->add_event(fd);
    }

    // We needn't delete the event on windows.
    ~IoEvent() = default;

    bool wait(int ms=-1) {
        if (ms < 0) { scheduler()->yield(); return true; }
        
        if (!_has_timer) { _has_timer = true; scheduler()->add_io_timer(ms); }
        scheduler()->yield();
        if (!scheduler()->timeout()) return true;

        CancelIo((HANDLE)_fd);
        WSASetLastError(ETIMEDOUT);
        return false;
    }

  private:
    sock_t _fd;
    bool _has_timer;
};

#else
class IoEvent {
  public:
    IoEvent(sock_t fd, int ev)
        : _fd(fd), _ev(ev), _has_timer(false), _has_ev(false) {
    }

    ~IoEvent() {
        if (_has_ev) scheduler()->del_event(_fd, _ev);
    }

    bool wait(int ms=-1) {
        if (!_has_ev) _has_ev = scheduler()->add_event(_fd, _ev);
        if (ms < 0) { scheduler()->yield(); return true; }

        if (!_has_timer) { _has_timer = true; scheduler()->add_io_timer(ms); }
        scheduler()->yield();
        if (!scheduler()->timeout()) return true;

        errno = ETIMEDOUT;
        return false;
    }

  private:
    sock_t _fd;
    int _ev;
    bool _has_timer;
    bool _has_ev;
};
#endif

} // namespace co

using co::go;
