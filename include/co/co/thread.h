#pragma once

#include "../def.h"
#include <assert.h>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace std {
typedef std::lock_guard<std::mutex> mutex_guard;
} // std

namespace co {
namespace xx {

#ifdef _WIN32
typedef DWORD tls_key_t;
inline void tls_init(tls_key_t* k) { *k = TlsAlloc(); assert(*k != TLS_OUT_OF_INDEXES); }
inline void tls_free(tls_key_t k) { TlsFree(k); }
inline void* tls_get(tls_key_t k) { return TlsGetValue(k); }
inline void tls_set(tls_key_t k, void* v) { BOOL r = TlsSetValue(k, v); assert(r); (void)r; }

#else
typedef pthread_key_t tls_key_t;
inline void tls_init(tls_key_t* k) { int r = pthread_key_create(k, 0); (void)r; assert(r == 0); }
inline void tls_free(tls_key_t k) { int r = pthread_key_delete(k); (void)r; assert(r == 0); }
inline void* tls_get(tls_key_t k) { return pthread_getspecific(k); }
inline void tls_set(tls_key_t k, void* v) { int r = pthread_setspecific(k, v); (void)r; assert(r == 0); }
__coapi extern __thread uint32 g_tid;
__coapi uint32 thread_id();
#endif
} // xx

#ifdef _WIN32
inline uint32 thread_id() { return GetCurrentThreadId(); }
#else
inline uint32 thread_id() {
    return xx::g_tid != 0 ? xx::g_tid : (xx::g_tid = xx::thread_id());
}
#endif

template<typename T>
class tls {
  public:
    tls() { xx::tls_init(&_key); }
    ~tls() { xx::tls_free(_key); }

    T* get() const { return (T*) xx::tls_get(_key); }

    void set(T* p) { xx::tls_set(_key, p); }

    T* operator->() const {
        T* const o = this->get(); assert(o);
        return o;
    }

    T& operator*() const {
        T* const o = this->get(); assert(o);
        return *o;
    }

    bool operator==(T* p) const { return this->get() == p; }
    bool operator!=(T* p) const { return this->get() != p; }
    explicit operator bool() const { return this->get() != 0; }

  private:
    xx::tls_key_t _key;
    DISALLOW_COPY_AND_ASSIGN(tls);
};

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
