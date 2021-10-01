#pragma once

#include "def.h"
#include "atomic.h"
#include "closure.h"
#include <assert.h>
#include <memory>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace co {
namespace xx {

typedef CRITICAL_SECTION mutex_t;
inline void mutex_init(mutex_t* m)     { InitializeCriticalSection(m); }
inline void mutex_destroy(mutex_t* m)  { DeleteCriticalSection(m); }
inline void mutex_lock(mutex_t* m)     { EnterCriticalSection(m); }
inline bool mutex_try_lock(mutex_t* m) { return TryEnterCriticalSection(m) != FALSE; }
inline void mutex_unlock(mutex_t* m)   { LeaveCriticalSection(m); }

typedef CONDITION_VARIABLE cond_t;
inline void cond_init(cond_t* c)                        { InitializeConditionVariable(c); }
inline void cond_destroy(cond_t*)                     {}
inline void cond_wait(cond_t* c, mutex_t* m)            { SleepConditionVariableCS(c, m, INFINITE); }
inline bool cond_wait(cond_t* c, mutex_t* m, uint32 ms) { return SleepConditionVariableCS(c, m, ms) == TRUE; }
inline void cond_notify(cond_t* c)                      { WakeAllConditionVariable(c); }

typedef HANDLE thread_t;
typedef DWORD (WINAPI *thread_fun_t)(void*);
#define _CO_DEF_THREAD_FUN(f, p) static DWORD WINAPI f(void* p)
inline uint32 thread_id()                                      { return GetCurrentThreadId(); }
inline void thread_start(thread_t* t, thread_fun_t f, void* p) { *t = CreateThread(0, 0, f, p, 0, 0); assert(*t); }
inline void thread_join(thread_t t)                            { WaitForSingleObject(t, INFINITE); CloseHandle(t); }
inline void thread_detach(thread_t t)                          { CloseHandle(t); }

typedef DWORD tls_t;
inline void tls_init(tls_t* k)        { *k = TlsAlloc(); assert(*k != TLS_OUT_OF_INDEXES); }
inline void tls_destroy(tls_t k)      { TlsFree(k); }
inline void* tls_get(tls_t k)         { return TlsGetValue(k); }
inline void tls_set(tls_t k, void* v) { BOOL r = TlsSetValue(k, v); assert(r); (void)r; }

} // xx
} // co

#else
#include <pthread.h>

namespace co {
namespace xx {

// mutex
typedef pthread_mutex_t mutex_t;
inline void mutex_init(mutex_t* m)     { int r = pthread_mutex_init(m, 0); assert(r == 0); }
inline void mutex_destroy(mutex_t* m)  { int r = pthread_mutex_destroy(m); assert(r == 0); }
inline void mutex_lock(mutex_t* m)     { int r = pthread_mutex_lock(m); assert(r == 0); }
inline void mutex_unlock(mutex_t* m)   { int r = pthread_mutex_unlock(m); assert(r == 0); }
inline bool mutex_try_lock(mutex_t* m) { return pthread_mutex_trylock(m) == 0; }

typedef pthread_cond_t cond_t;
void cond_init(cond_t* c);
bool cond_wait(cond_t* c, mutex_t* m, uint32 ms);
inline void cond_destroy(cond_t* c)          { pthread_cond_destroy(c); }
inline void cond_wait(cond_t* c, mutex_t* m) { pthread_cond_wait(c, m); }
inline void cond_notify(cond_t* c)           { pthread_cond_broadcast(c); }

typedef pthread_t thread_t;
typedef void* (*thread_fun_t)(void*);
#define _CO_DEF_THREAD_FUN(f, p) static void* f(void* p)
uint32 thread_id();
inline void thread_start(thread_t* t, thread_fun_t f, void* p) { int r = pthread_create(t, 0, f, p); assert(r == 0); }
inline void thread_join(thread_t t)                            { pthread_join(t, 0); }
inline void thread_detach(thread_t t)                          { pthread_detach(t); }

typedef pthread_key_t tls_t;
inline void tls_init(tls_t* k)        { int r = pthread_key_create(k, 0); assert(r == 0); }
inline void tls_destroy(tls_t k)      { int r = pthread_key_delete(k); assert(r == 0); }
inline void* tls_get(tls_t k)         { return pthread_getspecific(k); }
inline void tls_set(tls_t k, void* v) { int r = pthread_setspecific(k, v); assert(r == 0); }

} // xx
} // co

#endif

class __codec Mutex {
  public:
    Mutex() {
        co::xx::mutex_init(&_mutex);
    }

    ~Mutex() {
        co::xx::mutex_destroy(&_mutex);
    }

    void lock() {
        co::xx::mutex_lock(&_mutex);
    }

    void unlock() {
        co::xx::mutex_unlock(&_mutex);
    }

    bool try_lock() {
        return co::xx::mutex_try_lock(&_mutex);
    }

    co::xx::mutex_t* mutex() {
        return &_mutex;
    }

  private:
    co::xx::mutex_t _mutex;
    DISALLOW_COPY_AND_ASSIGN(Mutex);
};

class __codec MutexGuard {
  public:
    explicit MutexGuard(Mutex& lock) : _lock(lock) {
        _lock.lock();
    }

    explicit MutexGuard(Mutex* lock) : _lock(*lock) {
        _lock.lock();
    }

    ~MutexGuard() {
        _lock.unlock();
    }

  private:
    Mutex& _lock;
    DISALLOW_COPY_AND_ASSIGN(MutexGuard);
};

class __codec SyncEvent {
  public:
    explicit SyncEvent(bool manual_reset = false, bool signaled = false)
        : _counter(0), _manual_reset(manual_reset), _signaled(signaled) {
        co::xx::cond_init(&_cond);
    }

    ~SyncEvent() {
        co::xx::cond_destroy(&_cond);
    }

    void signal() {
        MutexGuard g(_mutex);
        if (!_signaled) {
            _signaled = true;
            co::xx::cond_notify(&_cond);
        }
    }

    void reset() {
        MutexGuard g(_mutex);
        _signaled = false;
    }

    void wait() {
        MutexGuard g(_mutex);
        if (!_signaled) {
            ++_counter;
            co::xx::cond_wait(&_cond, _mutex.mutex());
            --_counter;
            assert(_signaled);
        }
        if (!_manual_reset && _counter == 0) _signaled = false;
    }

    // return false if timeout
    bool wait(uint32 ms) {
        MutexGuard g(_mutex);
        if (!_signaled) {
            ++_counter;
            bool r = co::xx::cond_wait(&_cond, _mutex.mutex(), ms);
            --_counter;
            if (!r) return false;
            assert(_signaled);
        }
        if (!_manual_reset && _counter == 0) _signaled = false;
        return true;
    }

  private:
    co::xx::cond_t _cond;
    Mutex _mutex;
    int _counter;
    const bool _manual_reset;
    bool _signaled;
    DISALLOW_COPY_AND_ASSIGN(SyncEvent);
};

// starts a thread:
//   Thread x([]() {...});               // lambda
//   Thread x(f);                        // void f();
//   Thread x(f, p);                     // void f(void*);  void* p;
//   Thread x(&T::f, &t);                // void T::f();  T t;
//   Thread x(f, 7);                     // void f(int v);
//   Thread x(std::bind(&T::f, &t, 7));  // void T::f(int v);  T t;
//
// run independently from thread object:
//   Thread(f).detach();                 // void f();
class __codec Thread {
  public:
    // @cb is not saved in this thread object, but passed directly to the
    // thread function, so it can run independently from the thread object.
    explicit Thread(co::Closure* cb) : _t(0) {
        co::xx::thread_start(&_t, &Thread::_thread_fun, (void*)cb);
    }

    template<typename F>
    explicit Thread(F&& f)
        : Thread(co::new_closure(std::forward<F>(f))) {
    }

    template<typename F, typename P>
    Thread(F&& f, P&& p)
        : Thread(co::new_closure(std::forward<F>(f), std::forward<P>(p))) {
    }

    template<typename F, typename T, typename P>
    Thread(F&& f, T* t, P&& p)
        : Thread(co::new_closure(std::forward<F>(f), t, std::forward<P>(p))) {
    }

    ~Thread() {
        this->join();
    }

    // blocks until the thread function terminates
    void join() {
        co::xx::thread_t t = atomic_swap(&_t, (co::xx::thread_t)0);
        if (t != 0) co::xx::thread_join(t);
    }

    void detach() {
        co::xx::thread_t t = atomic_swap(&_t, (co::xx::thread_t)0);
        if (t != 0) co::xx::thread_detach(t);
    }

  private:
    co::xx::thread_t _t;
    DISALLOW_COPY_AND_ASSIGN(Thread);

    _CO_DEF_THREAD_FUN(_thread_fun, p) {
        co::Closure* cb = (co::Closure*) p;
        if (cb) cb->run();
        return 0;
    }
};

namespace co {
inline uint32 thread_id() {
    static __thread uint32 id = 0;
    if (id != 0) return id;
    return id = co::xx::thread_id();
}
} // co

// deprecated since v2.0.2, use co::thread_id() instead.
inline uint32 current_thread_id() {
    return co::thread_id();
}

// thread_ptr is based on TLS. Each thread sets and holds its own pointer.
// It is easy to use, just like the std::unique_ptr.
//   thread_ptr<T> pt;
//   if (!pt) pt.reset(new T);
template <typename T, typename D=std::default_delete<T>>
class thread_ptr {
  public:
    thread_ptr() {
        co::xx::tls_init(&_key);
    }

    ~thread_ptr() {
        co::xx::tls_destroy(_key);
    }

    T* get() const {
        return (T*) co::xx::tls_get(_key);
    }

    void reset(T* p = 0) {
        T* o = this->get();
        if (o != p) {
            if (o) D()(o);
            co::xx::tls_set(_key, p);
        }
    }

    void operator=(T* p) {
        this->reset(p);
    }

    T* release() {
        T* o = this->get();
        co::xx::tls_set(_key, 0);
        return o;
    }

    T* operator->() const {
        T* o = this->get(); assert(o);
        return o;
    }

    T& operator*() const {
        T* o = this->get(); assert(o);
        return *o;
    }

    bool operator==(T* p) const {
        return this->get() == p;
    }

    bool operator!=(T* p) const {
        return this->get() != p;
    }

    bool operator!() const {
        return this->get() == 0;
    }

    explicit operator bool() const {
        return this->get() != 0;
    }

  private:
    co::xx::tls_t _key;
    DISALLOW_COPY_AND_ASSIGN(thread_ptr);
};
