#pragma once

#ifdef _WIN32

#include "../atomic.h"
#include "../closure.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <assert.h>
#include <memory>

class Mutex {
  public:
    Mutex() {
        InitializeCriticalSection(&_mutex);
    }

    ~Mutex() {
        DeleteCriticalSection(&_mutex);
    }

    void lock() {
        EnterCriticalSection(&_mutex);
    }

    void unlock() {
        LeaveCriticalSection(&_mutex);
    }

    bool try_lock() {
        return TryEnterCriticalSection(&_mutex) != FALSE;
    }

    CRITICAL_SECTION* mutex() {
        return &_mutex;
    }

  private:
    CRITICAL_SECTION _mutex;
    DISALLOW_COPY_AND_ASSIGN(Mutex);
};

template<typename T>
class LockGuard {
  public:
    explicit LockGuard(T& lock) : _lock(lock) {
        _lock.lock();
    }

    explicit LockGuard(T* lock) : _lock(*lock) {
        _lock.lock();
    }

    ~LockGuard() {
        _lock.unlock();
    }

  private:
    T& _lock;
    DISALLOW_COPY_AND_ASSIGN(LockGuard);
};

typedef LockGuard<Mutex> MutexGuard;

class SyncEvent {
  public:
    explicit SyncEvent(bool manual_reset = false, bool signaled = false) {
        _h = CreateEvent(0, manual_reset, signaled, 0);
        assert(_h != INVALID_HANDLE_VALUE);
    }

    ~SyncEvent() {
        CloseHandle(_h);
    }

    void signal() {
        SetEvent(_h);
    }

    void reset() {
        ResetEvent(_h);
    }

    void wait() {
        WaitForSingleObject(_h, INFINITE);
    }

    // Event is signaled  ->  WAIT_OBJECT_0    return true.
    // Timeout            ->  WAIT_TIMEOUT     return false.
    bool wait(unsigned int ms) {
        return WaitForSingleObject(_h, ms) == WAIT_OBJECT_0;
    }

  private:
    HANDLE _h;
    DISALLOW_COPY_AND_ASSIGN(SyncEvent);
};

// starts a thread:
//   Thread x(f);                        // void f();
//   Thread x(f, p);                     // void f(void*);  void* p;
//   Thread x(&T::f, &t);                // void T::f();  T t;
//   Thread x(std::bind(f, 7));          // void f(int v);
//   Thread x(std::bind(&T::f, &t, 7));  // void T::f(int v);  T t;
//
// run independently from thread object:
//   Thread(f).detach();                 // void f();
class Thread {
  public:
    explicit Thread(Closure* cb) {
        // @cb is not saved in this thread object, but passed directly to the 
        // thread function, so it can run independently from the thread object.
        _h = CreateThread(0, 0, &Thread::_Run, cb, 0, 0);
        assert(_h != INVALID_HANDLE_VALUE);
    }

    explicit Thread(void (*f)())
        : Thread(new_callback(f)) {
    }

    Thread(void (*f)(void*), void* p)
        : Thread(new_callback(f, p)) {
    }

    template<typename T>
    Thread(void (T::*f)(), T* p)
        : Thread(new_callback(f, p)) {
    }

    explicit Thread(std::function<void()>&& f)
        : Thread(new_callback(std::move(f))) {
    }

    explicit Thread(const std::function<void()>& f)
        : Thread(new_callback(f)) {
    }

    ~Thread() {
        this->join();
    }

    // Wait until the thread function terminates.
    void join() {
        HANDLE h = atomic_swap(&_h, INVALID_HANDLE_VALUE);
        if (h != INVALID_HANDLE_VALUE) {
            WaitForSingleObject(h, INFINITE);
            CloseHandle(h);
        }
    }

    void detach() {
        HANDLE h = atomic_swap(&_h, INVALID_HANDLE_VALUE);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }

  private:
    HANDLE _h;
    DISALLOW_COPY_AND_ASSIGN(Thread);

    static DWORD WINAPI _Run(void* p) {
        Closure* cb = (Closure*) p;
        if (cb) cb->run();
        return 0;
    }
};

inline unsigned int current_thread_id() {
    static __thread unsigned int id = 0;
    if (id != 0) return id;
    return id = GetCurrentThreadId();
}

// thread_ptr is a kind of smart pointer based on TLS. Each thread sets and 
// holds its own pointer. It is easy to use, just like the std::unique_ptr.
//   thread_ptr<T> pt;
//   if (pt == 0) pt.reset(new T);
template <typename T>
class thread_ptr {
  public:
    thread_ptr() {
        _key = TlsAlloc();
        assert(_key != TLS_OUT_OF_INDEXES);
    }

    ~thread_ptr() {
        TlsFree(_key);
        for (auto it = _objs.begin(); it != _objs.end(); ++it) {
            delete it->second;
        }
    }

    T* get() const {
        return (T*) TlsGetValue(_key);
    }

    void reset(T* p = 0) {
        T* obj = this->get();
        if (obj == p) return;

        delete obj;
        TlsSetValue(_key, p);

        {
            MutexGuard g(_mtx);
            _objs[current_thread_id()] = p;
        }
    }

    void operator=(T* p) {
        this->reset(p);
    }

    T* release() {
        T* obj = this->get();
        TlsSetValue(_key, 0);
        {
            MutexGuard g(_mtx);
            _objs[current_thread_id()] = 0;
        }
        return obj;
    }

    T* operator->() const {
        T* obj = this->get();
        assert(obj);
        return obj;
    }

    T& operator*() const {
        T* obj = this->get();
        assert(obj);
        return *obj;
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

  private:
    DWORD _key;

    Mutex _mtx;
    std::unordered_map<unsigned int, T*> _objs;

    DISALLOW_COPY_AND_ASSIGN(thread_ptr);
};

#endif
