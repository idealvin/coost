#ifndef _WIN32
#include "co/thread.h"

#ifdef __linux__
#include <unistd.h>       // for syscall()
#include <sys/syscall.h>  // for SYS_xxx definitions
#include <time.h>         // for clock_gettime
#else
#include <sys/time.h>     // for gettimeofday
#endif

namespace co {
namespace xx {

void cond_init(cond_t* c) {
  #ifdef __linux__
    pthread_condattr_t attr;
    int r = pthread_condattr_init(&attr); assert(r == 0);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    r = pthread_cond_init(c, &attr); assert(r == 0);
    pthread_condattr_destroy(&attr);
  #else
    int r = pthread_cond_init(c, 0); assert(r == 0);
  #endif
}

bool cond_wait(cond_t* c, mutex_t* m, uint32 ms) {
  #ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
  #else
    struct timespec ts;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
  #endif

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

#ifdef __linux__
#ifndef SYS_gettid
#define SYS_gettid __NR_gettid
#endif

uint32 thread_id() {
    return syscall(SYS_gettid);
}

#else /* for mac, bsd.. */
uint32 thread_id() {
    uint64 x;
    pthread_threadid_np(0, &x);
    return (uint32)x;
}
#endif

} // xx
} // co

#endif
