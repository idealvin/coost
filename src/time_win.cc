#ifdef _WIN32

#include "co/time.h"
#include <time.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace co {
namespace now {
namespace xx {

inline int64 _query_counts() {
    LARGE_INTEGER x;
    QueryPerformanceCounter(&x);
    return x.QuadPart;
}

inline const int64& _counts_per_sec() {
    static const int64 freq = [](){
        LARGE_INTEGER x;
        QueryPerformanceFrequency(&x);
        return x.QuadPart;
    }();
    return freq;
}

inline int64 ns() {
    const int64 count = _query_counts();
    const int64& freq = _counts_per_sec();
    return (int64)(static_cast<double>(count) * 1000000000 / freq);
}

inline int64 us() {
    const int64 count = _query_counts();
    const int64& freq = _counts_per_sec();
    return (int64)(static_cast<double>(count) * 1000000 / freq);
}

inline int64 ms() {
    const int64 count = _query_counts();
    const int64& freq = _counts_per_sec();
    return (int64)(static_cast<double>(count) * 1000 / freq);
}

} // xx

int64 ns() {
    return xx::ns();
}

int64 us() {
    return xx::us();
}

int64 ms() {
    return xx::ms();
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
} // co

namespace _xx {
namespace sleep {

void ms(uint32 n) {
    ::Sleep(n);
}

void sec(uint32 n) {
    ::Sleep(n * 1000);
}

} // sleep
} // _xx

#endif
