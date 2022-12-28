#pragma once

#include "../def.h"
#include <assert.h>
#include <memory>
#include <mutex>
#include <thread>
#include <functional>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif


namespace co {
namespace xx {
__coapi uint32 thread_id();

#ifdef _WIN32
typedef DWORD tls_key_t;
inline void tls_init(tls_key_t* k) { *k = TlsAlloc(); assert(*k != TLS_OUT_OF_INDEXES); }
inline void tls_free(tls_key_t k) { TlsFree(k); }
inline void* tls_get(tls_key_t k) { return TlsGetValue(k); }
inline void tls_set(tls_key_t k, void* v) { BOOL r = TlsSetValue(k, v); assert(r); (void)r; }

#else
typedef pthread_key_t tls_key_t;
inline void tls_init(tls_key_t* k) { int r = pthread_key_create(k, 0); assert(r == 0); }
inline void tls_free(tls_key_t k) { int r = pthread_key_delete(k); assert(r == 0); }
inline void* tls_get(tls_key_t k) { return pthread_getspecific(k); }
inline void tls_set(tls_key_t k, void* v) { int r = pthread_setspecific(k, v); assert(r == 0); }
#endif
} // xx

// get id of the current thread
inline uint32 thread_id() {
    static __thread uint32 id = 0;
    return id != 0 ? id : (id = xx::thread_id());
}

// thread_ptr is based on TLS. Each thread sets and holds its own pointer.
// It is easy to use, just like the std::unique_ptr.
//   thread_ptr<T> pt;
//   if (!pt) pt.reset(new T);
template <typename T, typename D=std::default_delete<T>>
class thread_ptr {
  public:
    thread_ptr() { xx::tls_init(&_key); }
    ~thread_ptr() { xx::tls_free(_key); }

    T* get() const { return (T*) xx::tls_get(_key); }

    void reset(T* p = 0) {
        T* o = this->get();
        if (o != p) {
            if (o) D()(o);
            xx::tls_set(_key, p);
        }
    }

    void operator=(T* p) { this->reset(p); }

    T* release() {
        T* o = this->get();
        xx::tls_set(_key, 0);
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

    bool operator==(T* p) const { return this->get() == p; }
    bool operator!=(T* p) const { return this->get() != p; }
    bool operator!() const { return this->get() == 0; }
    explicit operator bool() const { return this->get() != 0; }

  private:
    xx::tls_key_t _key;
    DISALLOW_COPY_AND_ASSIGN(thread_ptr);
};

// for threads only
class __coapi sync_event {
  public:
    explicit sync_event(bool manual_reset = false, bool signaled = false);
    ~sync_event();

    sync_event(sync_event&& e) noexcept : _p(e._p) {
        e._p = 0;
    }

    void signal();
    void reset();
    void wait();
    bool wait(uint32 ms);

  private:
    void* _p;
    DISALLOW_COPY_AND_ASSIGN(sync_event);
};

} // co
