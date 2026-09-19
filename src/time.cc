#include "co/time.h"
#include <time.h>
#include <chrono>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/time.h>
#endif // ifdef _WIN32

namespace co {
namespace xx {

int64 MonoTime::ns() {
    return static_cast<int64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

#ifdef _WIN32
inline int64 _filetime() {
    FILETIME ft;
    LARGE_INTEGER x;
    GetSystemTimeAsFileTime(&ft);
    x.LowPart = ft.dwLowDateTime;
    x.HighPart = ft.dwHighDateTime;
    return x.QuadPart - 116444736000000000ULL;
}

int64 Now::ns() {
    return _filetime() * 100;
}

int64 Now::us() {
    return _filetime() / 10;
}

int64 Now::ms() {
    return _filetime() / 10000;
}

co::string Now::str(const char* fmt) {
    int64 x = ::time(0);
    struct tm t;
    _localtime64_s(&t, &x);

    char buf[256];
    const size_t r = strftime(buf, sizeof(buf), fmt, &t);
    return co::string(buf, r);
}

#else
int64 Now::ns() {
    struct timeval t;
    gettimeofday(&t, 0);
    return static_cast<int64>(t.tv_sec) * 1000000000 + t.tv_usec * 1000;
}

int64 Now::us() {
    struct timeval t;
    gettimeofday(&t, 0);
    return static_cast<int64>(t.tv_sec) * 1000000 + t.tv_usec;
}

int64 Now::ms() {
    struct timeval t;
    gettimeofday(&t, 0);
    return static_cast<int64>(t.tv_sec) * 1000 + t.tv_usec / 1000;
}

co::string Now::str(const char* fmt) {
    time_t x = ::time(0);
    struct tm t;
    localtime_r(&x, &t);

    char buf[256];
    const size_t r = strftime(buf, sizeof(buf), fmt, &t);
    return co::string(buf, r);
}
#endif // ifdef _WIN32

} // xx
} // co

namespace __co {
namespace time {

#ifdef _WIN32
void sleep(uint32 ms) { ::Sleep(ms); }

#else
void sleep(uint32 ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = ms % 1000 * 1000000;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR);
}
#endif // ifdef _WIN32

} // time
} // __co
