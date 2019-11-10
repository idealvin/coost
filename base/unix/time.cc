#ifndef _WIN32

#include "../time.h"
#include <time.h>
#include <sys/time.h>

namespace now {

struct _Mono {
  #ifdef CLOCK_MONOTONIC
    static inline int64 ms() {
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        return static_cast<int64>(t.tv_sec) * 1000 + t.tv_nsec / 1000000;
    }

    static inline int64 us() {
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        return static_cast<int64>(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
    }
  #else
    static inline int64 ms() {
        struct timeval t;
        gettimeofday(&t, 0);
        return static_cast<int64>(t.tv_sec) * 1000 + t.tv_usec / 1000;
    }

    static inline int64 us() {
        struct timeval t;
        gettimeofday(&t, 0);
        return static_cast<int64>(t.tv_sec) * 1000000 + t.tv_usec;
    }
  #endif
};

int64 ms() {
    return _Mono::ms();
}

int64 us() {
    return _Mono::us();
}

fastring str(const char* fm) {
    time_t x = time(0);
    struct tm t;
    localtime_r(&x, &t);

    char buf[32]; // 32 is big enough in most cases
    size_t r = strftime(buf, 32, fm, &t);
    return fastring(buf, r);
}

} // now

namespace ___ {
namespace sleep {

void ms(unsigned int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = ms % 1000 * 1000000;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR);
}

void sec(unsigned int s) {
    return sleep::ms(s * 1000);
}

} // sleep
} // ___

#endif
