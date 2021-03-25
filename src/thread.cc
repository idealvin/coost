#ifndef _WIN32
#include "co/thread.h"

#ifdef __linux__
#include <unistd.h>       // for syscall()
#include <sys/syscall.h>  // for SYS_xxx definitions
#include <time.h>         // for clock_gettime
#else
#include <errno.h>        // ETIMEDOUT for mac
#include <sys/time.h>     // for gettimeofday
#endif

namespace xx {

#ifdef __linux__
#ifndef SYS_gettid
#define SYS_gettid __NR_gettid
#endif

unsigned int gettid() {
    return syscall(SYS_gettid);
}

#else /* for mac, bsd */
unsigned int gettid() {
    uint64 x;
    pthread_threadid_np(0, &x);
    return (unsigned int) x;
}
#endif

} // xx

SyncEvent::SyncEvent(bool manual_reset, bool signaled)
    : _consumer(0), _manual_reset(manual_reset), _signaled(signaled) {
  #ifdef __linux__
    pthread_condattr_t attr;
    int r = pthread_condattr_init(&attr);
    assert(r == 0);

    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    r = pthread_cond_init(&_cond, &attr);
    assert(r == 0);

    pthread_condattr_destroy(&attr);
  #else
    int r = pthread_cond_init(&_cond, NULL);
    assert(r == 0);
  #endif
}

void SyncEvent::wait() {
    MutexGuard g(_mutex);
    if (!_signaled) {
        ++_consumer;
        pthread_cond_wait(&_cond, _mutex.mutex());
        --_consumer;
        assert(_signaled);
    }
    if (!_manual_reset && _consumer == 0) _signaled = false;
}

bool SyncEvent::wait(unsigned int ms) {
    MutexGuard g(_mutex);
    if (!_signaled) {
      #ifdef __linux__
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
      #else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct timespec ts;
        ts.tv_sec = tv.tv_sec;
        ts.tv_nsec = tv.tv_usec * 1000;
      #endif

        if (ms < 1000) {
            ts.tv_nsec += ms * 1000000;
        } else {
            unsigned int x = ms / 1000;
            ts.tv_nsec += (ms - x * 1000) * 1000000;
            ts.tv_sec += x;
        }

        if (ts.tv_nsec > 999999999) {
            ts.tv_nsec -= 1000000000;
            ++ts.tv_sec;
        }

        ++_consumer;
        int r = pthread_cond_timedwait(&_cond, _mutex.mutex(), &ts);
        --_consumer;

        if (r == ETIMEDOUT) return false;
        assert(r == 0);
        assert(_signaled);
    }

    if (!_manual_reset && _consumer == 0) _signaled = false;
    return true;
}
#endif
