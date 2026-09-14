#pragma once

#include "co/def.h"

#ifdef _WIN32
#include <intrin.h>
#endif

namespace co {

#ifndef _WIN32
#if __arch64
// find the least significant bit
inline int find_lsb(size_t x) {
    return __builtin_ffsll(x) - 1;
}

// find the most significant bit, x != 0
inline int find_msb(size_t x) {
    return 63 - __builtin_clzll(x);
}

#else
// find the least significant bit
inline int find_lsb(size_t x) {
    return __builtin_ffs(x) - 1;
}

// find the most significant bit, x != 0
inline int find_msb(size_t x) {
    return 31 - __builtin_clz(x);
}
#endif

#else
#if __arch64
// find the least significant bit
inline int find_lsb(size_t x) {
    unsigned long r;
    return _BitScanForward64(&r, x) ? (int)r : -1;
}

// find the most significant bit, x != 0
inline int find_msb(size_t x) {
    unsigned long r;
    _BitScanReverse64(&r, x);
    return (int)r;
}

#else
// find the least significant bit
inline int find_lsb(size_t x) {
    unsigned long r;
    return _BitScanForward(&r, x) ? (int)r : -1;
}

// find the most significant bit, x != 0
inline int find_msb(size_t x) {
    unsigned long i;
    _BitScanReverse(&i, x);
    return (int)i;
}
#endif

#endif

} // co
