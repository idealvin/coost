#ifdef _WIN32

#include "co/time.h"
#include <time.h>
#include <winsock2.h>  // for struct timeval

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace now {
namespace _Mono {

inline int64 _QueryFrequency() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

inline int64 _QueryCounter() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

inline const int64& _Frequency() {
    static int64 freq = _QueryFrequency();
    return freq;
}

inline int64 ms() {
    int64 count = _QueryCounter();
    const int64& freq = _Frequency();
    return (count / freq) * 1000 + (count % freq * 1000 / freq);
}

inline int64 us() {
    int64 count = _QueryCounter();
    const int64& freq = _Frequency();
    return (count / freq) * 1000000 + (count % freq * 1000000 / freq);
}

} // _Mono

int64 ms() {
    return _Mono::ms();
}

int64 us() {
    return _Mono::us();
}

fastring str(const char* fm) {
    int64 x = time(0);
    struct tm t;
    _localtime64_s(&t, &x);

    char buf[256];
    const size_t r = strftime(buf, sizeof(buf), fm, &t);
    return fastring(buf, r);
}

} // now

namespace epoch {

inline int64 filetime() {
    FILETIME ft;
    LARGE_INTEGER x;
    GetSystemTimeAsFileTime(&ft);
    x.LowPart = ft.dwLowDateTime;
    x.HighPart = ft.dwHighDateTime;
    return x.QuadPart - 116444736000000000ULL;
}

int64 ms() {
    return filetime() / 10000;
}

int64 us() {
    return filetime() / 10;
}

} // epoch

namespace ___ {
namespace sleep {

void ms(uint32 n) {
    ::Sleep(n);
}

void sec(uint32 n) {
    ::Sleep(n * 1000);
}

} // sleep
} // ___

#endif
