#ifdef _WIN32

#include "co/time.h"
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

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

    fastring s(32); // 32 is big enough in most cases
    const size_t r = strftime((char*)s.data(), 32, fm, &t);
    s.resize(r);
    return s;
}

} // now

namespace ___ {
namespace sleep {

void ms(unsigned int n) {
    ::Sleep(n);
}

void sec(unsigned int n) {
    ::Sleep(n * 1000);
}

} // sleep
} // ___

#endif
