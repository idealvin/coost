#ifndef _WIN32

#include "co/time.h"
#include <time.h>
#include <sys/time.h>

namespace now {
namespace _Mono {

#ifdef CLOCK_MONOTONIC

inline int64 ns() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<int64>(t.tv_sec) * 1000000000 + t.tv_nsec;
}

inline int64 us() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<int64>(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}

inline int64 ms() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<int64>(t.tv_sec) * 1000 + t.tv_nsec / 1000000;
}

#else

// WARNING:
//   If you are from the year 2262 or later, DO NOT use this,
//   as nanoseconds since epoth (1970/1/1) may overflow then.
inline int64 ns() { return epoch::us() * 1000; }

inline int64 us() { return epoch::us(); }

inline int64 ms() { return epoch::ms(); }

#endif

} // _Mono

int64 ns() {
    return _Mono::ns();
}

int64 us() {
    return _Mono::us();
}

int64 ms() {
    return _Mono::ms();
}

fastring str(const char* fm) {
    time_t x = time(0);
    struct tm t;
    localtime_r(&x, &t);

    char buf[256];
    const size_t r = strftime(buf, sizeof(buf), fm, &t);
    return fastring(buf, r);
}

} // now

namespace epoch {

int64 us() {
    struct timeval t;
    gettimeofday(&t, 0);
    return static_cast<int64>(t.tv_sec) * 1000000 + t.tv_usec;
}

int64 ms() {
    struct timeval t;
    gettimeofday(&t, 0);
    return static_cast<int64>(t.tv_sec) * 1000 + t.tv_usec / 1000;
}

} // epoch

namespace ___ {
namespace sleep {

void ms(uint32 n) {
    struct timespec ts;
    ts.tv_sec = n / 1000;
    ts.tv_nsec = n % 1000 * 1000000;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR);
}

void sec(uint32 n) {
    struct timespec ts;
    ts.tv_sec = n;
    ts.tv_nsec = 0;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR);
}

} // sleep
} // ___

#endif
