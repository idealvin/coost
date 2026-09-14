#include "co/time.h"
#include <time.h>

#ifndef _WIN32
#include <sys/time.h>

namespace _xx {
namespace time {
namespace xx {

int64 Unix::ns() {
    struct timeval t;
    gettimeofday(&t, 0);
    return static_cast<int64>(t.tv_sec) * 1000000000 + t.tv_usec * 1000;
}

int64 Unix::us() {
    struct timeval t;
    gettimeofday(&t, 0);
    return static_cast<int64>(t.tv_sec) * 1000000 + t.tv_usec;
}

int64 Unix::ms() {
    struct timeval t;
    gettimeofday(&t, 0);
    return static_cast<int64>(t.tv_sec) * 1000 + t.tv_usec / 1000;
}

#ifdef CLOCK_MONOTONIC

int64 Mono::ns() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<int64>(t.tv_sec) * 1000000000 + t.tv_nsec;
}

int64 Mono::us() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<int64>(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}

int64 Mono::ms() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<int64>(t.tv_sec) * 1000 + t.tv_nsec / 1000000;
}

#else

int64 Mono::ns() {
    return Unix::ns();
}

int64 Mono::us() {
    return Unix::us();
}

int64 Mono::ms() {
    return Unix::ms();
}

#endif

} // xx

void sleep(uint32 ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = ms % 1000 * 1000000;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR);
}

co::string str(const char* fmt) {
    time_t x = ::time(0);
    struct tm t;
    localtime_r(&x, &t);

    char buf[256];
    const size_t r = strftime(buf, sizeof(buf), fmt, &t);
    return co::string(buf, r);
}

} // time
} // _xx

#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace _xx {
namespace time {
namespace xx {

static int g_nifty_counter;
static int64 g_freq;

TimeInit::TimeInit() {
    if (g_nifty_counter++ == 0) {
        LARGE_INTEGER x;
        QueryPerformanceFrequency(&x);
        g_freq = x.QuadPart;
    }
}

inline int64 _filetime() {
    FILETIME ft;
    LARGE_INTEGER x;
    GetSystemTimeAsFileTime(&ft);
    x.LowPart = ft.dwLowDateTime;
    x.HighPart = ft.dwHighDateTime;
    return x.QuadPart - 116444736000000000ULL;
}

int64 Unix::ns() {
    return _filetime() * 100;
}

int64 Unix::us() {
    return _filetime() / 10;
}

int64 Unix::ms() {
    return _filetime() / 10000;
}

inline int64 _query_counts() {
    LARGE_INTEGER x;
    QueryPerformanceCounter(&x);
    return x.QuadPart;
}

int64 Mono::ns() {
    const int64 count = _query_counts();
    return (int64)(static_cast<double>(count) * 1000000000 / g_freq);
}

int64 Mono::us() {
    const int64 count = _query_counts();
    return (int64)(static_cast<double>(count) * 1000000 / g_freq);
}

int64 Mono::ms() {
    const int64 count = _query_counts();
    return (int64)(static_cast<double>(count) * 1000 / g_freq);
}

} // xx

void sleep(uint32 ms) {
    ::Sleep(ms);
}

co::string str(const char* fmt) {
    int64 x = ::time(0);
    struct tm t;
    _localtime64_s(&t, &x);

    char buf[256];
    const size_t r = strftime(buf, sizeof(buf), fmt, &t);
    return co::string(buf, r);
}

} // time
} // _xx

#endif
