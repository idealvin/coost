#pragma once

#include "co/mem.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef CRITICAL_SECTION _mutex_t;
typedef CONDITION_VARIABLE _cv_t;

inline void _mutex_init(_mutex_t* m) { InitializeCriticalSection(m); }
inline void _mutex_free(_mutex_t* m) { DeleteCriticalSection(m); }
inline void _mutex_lock(_mutex_t* m) { EnterCriticalSection(m); }
inline void _mutex_unlock(_mutex_t* m) { LeaveCriticalSection(m); }
inline bool _mutex_try_lock(_mutex_t* m) {
    return TryEnterCriticalSection(m) != FALSE;
}

inline void _cv_init(_cv_t* c) { InitializeConditionVariable(c); }
inline void _cv_free(_cv_t*) {}

inline void _cv_wait(_cv_t* c, _mutex_t* m) {
    SleepConditionVariableCS(c, m, INFINITE);
}

inline bool _cv_wait(_cv_t* c, _mutex_t* m, uint32 ms) {
    return SleepConditionVariableCS(c, m, ms) == TRUE;
}

inline void _cv_notify_one(_cv_t* c) { WakeConditionVariable(c); }
inline void _cv_notify_all(_cv_t* c) { WakeAllConditionVariable(c); }

#else
#include <pthread.h>

typedef pthread_mutex_t _mutex_t;
typedef pthread_cond_t _cv_t;

inline void _mutex_init(_mutex_t* m) {
    const int r = pthread_mutex_init(m, 0);
    runtime_assert(r == 0);
}

inline void _mutex_free(_mutex_t* m) {
    const int r = pthread_mutex_destroy(m);
    runtime_assert(r == 0);
}

inline void _mutex_lock(_mutex_t* m) {
    const int r = pthread_mutex_lock(m);
    runtime_assert(r == 0);
}

inline void _mutex_unlock(_mutex_t* m) {
    const int r = pthread_mutex_unlock(m);
    runtime_assert(r == 0);
}

inline bool _mutex_try_lock(_mutex_t* m) {
    return pthread_mutex_trylock(m) == 0;
}

#if defined(__linux__) || defined(__FreeBSD__)
#include <time.h> // for clock_gettime

inline void _cv_init(_cv_t* c) {
    pthread_condattr_t attr;
    int r = pthread_condattr_init(&attr);
    runtime_assert(r == 0);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    r = pthread_cond_init(c, &attr);
    runtime_assert(r == 0);
    pthread_condattr_destroy(&attr);
}

inline bool _cv_wait(_cv_t* c, _mutex_t* m, uint32 ms) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ms < 1000) {
        ts.tv_nsec += ms * 1000000;
    } else {
        const uint32 x = ms / 1000;
        ts.tv_nsec += (ms - x * 1000) * 1000000;
        ts.tv_sec += x;
    }
    if (ts.tv_nsec > 999999999) {
        ts.tv_nsec -= 1000000000;
        ++ts.tv_sec;
    }
    return pthread_cond_timedwait(c, m, &ts) == 0;
}

#else
#include <sys/time.h> // for gettimeofday

inline void _cv_init(_cv_t* c) {
    int r = pthread_cond_init(c, 0);
    runtime_assert(r == 0);
}

inline bool _cv_wait(_cv_t* c, _mutex_t* m, uint32 ms) {
    struct timespec ts;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
    if (ms < 1000) {
        ts.tv_nsec += ms * 1000000;
    } else {
        const uint32 x = ms / 1000;
        ts.tv_nsec += (ms - x * 1000) * 1000000;
        ts.tv_sec += x;
    }
    if (ts.tv_nsec > 999999999) {
        ts.tv_nsec -= 1000000000;
        ++ts.tv_sec;
    }
    return pthread_cond_timedwait(c, m, &ts) == 0;
}
#endif

inline void _cv_free(_cv_t* c) { pthread_cond_destroy(c); }
inline void _cv_wait(_cv_t* c, _mutex_t* m) { pthread_cond_wait(c, m); }
inline void _cv_notify_one(_cv_t* c) { pthread_cond_signal(c); }
inline void _cv_notify_all(_cv_t* c) { pthread_cond_broadcast(c); }

#endif

struct __cacheline_aligned _cv {
    _cv() : pred(false) { _cv_init(&cv); }
    ~_cv() { _cv_free(&cv); }

    void wait(_mutex_t* m) { _cv_wait(&cv, m); }
    bool wait(_mutex_t* m, uint32 ms) { return _cv_wait(&cv, m, ms); }
    void notify_one() { _cv_notify_one(&cv); }
    void notify_all() { _cv_notify_all(&cv); }

    _cv_t cv;
    bool pred;
};

extern __thread _cv* g_cv;

inline _cv* _get_cv() {
    const auto x = g_cv;
    return x ? x : (g_cv = co::_make_rootic<_cv>());
}

struct _mutex {
    _mutex() { _mutex_init(&_m); }
    ~_mutex() { _mutex_free(&_m); }

    void lock() { _mutex_lock(&_m); }
    void unlock() { _mutex_unlock(&_m); }
    bool try_lock() { return _mutex_try_lock(&_m); }
    _mutex_t* native_handle() { return &_m; }

    _mutex_t _m;
};

struct _mutex_guard {
    explicit _mutex_guard(_mutex& m) : _m(m) { _m.lock(); }
    ~_mutex_guard() { _m.unlock(); }

    _mutex& _m;
};
