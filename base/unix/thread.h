#pragma once

#ifndef _WIN32

#include "../atomic.h"
#include "../closure.h"

#include <pthread.h>
#include <assert.h>
#include <memory>
#ifdef __linux__
#include <sys/types.h>
#endif

class Mutex {
  public:
    Mutex() {
        int r = pthread_mutex_init(&_mutex, 0);
        assert(r == 0);
    }

    ~Mutex() {
        int r = pthread_mutex_destroy(&_mutex);
        assert(r == 0);
    }

    void lock() {
        int r = pthread_mutex_lock(&_mutex);
        assert(r == 0);
    }

    void unlock() {
        int r = pthread_mutex_unlock(&_mutex);
        assert(r == 0);
    }

    bool try_lock() {
        return pthread_mutex_trylock(&_mutex) == 0;
    }

    pthread_mutex_t* mutex() {
        return &_mutex;
    }

  private:
    pthread_mutex_t _mutex;
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
    explicit SyncEvent(bool manual_reset=false, bool signaled=false);

    ~SyncEvent() {
        pthread_cond_destroy(&_cond);
    }

    void signal() {
        MutexGuard g(_mutex);
        if (!_signaled) {
            _signaled = true;
            pthread_cond_broadcast(&_cond);
        }
    }

    void reset() {
        MutexGuard g(_mutex);
        _signaled = false;
    }

    void wait();

    // return false if timeout
    bool wait(unsigned int ms);

  private:
    pthread_cond_t _cond;
    Mutex _mutex;

    int _consumer;
    const bool _manual_reset;
    bool _signaled;

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
    // @cb is not saved in this thread object, but passed directly to the
    // thread function, so it can run independently from the thread object.
    explicit Thread(Closure* cb) : _id(0) {
        int r = pthread_create(&_id, 0, &Thread::_Run, cb);
        assert(r == 0);
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

    // wait until the thread function terminates
    void join() {
        pthread_t id = atomic_swap(&_id, (pthread_t)0);
        if (id != 0) pthread_join(id, 0);
    }

    void detach() {
        pthread_t id = atomic_swap(&_id, (pthread_t)0);
        if (id != 0) pthread_detach(id);
    }

  private:
    pthread_t _id;
    DISALLOW_COPY_AND_ASSIGN(Thread);

    static void* _Run(void* p) {
        Closure* cb = (Closure*) p;
        if (cb) cb->run();
        return 0;
    }
};

namespace xx {
unsigned int __gettid(); // get current thread id
} // xx

inline unsigned int __gettid() {
    static __thread unsigned int id = 0;
    if (id != 0) return id;
    return id = xx::__gettid();
}

#define gettid __gettid

// thread_ptr is based on TLS. Each thread sets and holds its own pointer.
// It is easy to use, just like the std::unique_ptr.
//   thread_ptr<T> pt;
//   if (!pt) pt.reset(new T);
template <typename T>
class thread_ptr {
  public:
    thread_ptr() {
        int r = pthread_key_create(&_key, 0);
        assert(r == 0);
    }

    ~thread_ptr() {
        int r = pthread_key_delete(_key);
        assert(r == 0);

        for (auto it = _objs.begin(); it != _objs.end(); ++it) {
            delete it->second;
        }
    }

    T* get() const {
        return (T*) pthread_getspecific(_key);
    }

    void reset(T* p = 0) {
        T* obj = this->get();
        if (obj == p) return;

        delete obj;
        pthread_setspecific(_key, p);

        {
            MutexGuard g(_mtx);
            _objs[gettid()] = p;
        }
    }

    void operator=(T* p) {
        this->reset(p);
    }

    T* release() {
        T* obj = this->get();
        pthread_setspecific(_key, 0);
        {
            MutexGuard g(_mtx);
            _objs[gettid()] = 0;
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
    pthread_key_t _key;

    Mutex _mtx;
    std::unordered_map<unsigned int, T*> _objs;

    DISALLOW_COPY_AND_ASSIGN(thread_ptr);
};

#endif
