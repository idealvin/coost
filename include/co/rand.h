#pragma once

#include "string.h"

namespace co {

// return a random number(0 < x < 2^31-1), thread-safe
uint32 rand();

// return a random number(0 < x < 2^31-1) with specific seed
// - @seed: 0 < seed < 2^31-1
inline uint32 rand(uint32& seed) {
    static const uint32 M = 0x7fffffffu;  // 2^31-1
    static const uint64 A = 16385;        // 2^14+1
    const uint64 p = seed * A;
    seed = static_cast<uint32>((p >> 31) + (p & M));
    return seed < M ? seed : (seed -= M);
}

uint64 rand64();

// splitmix64
inline uint64 rand64(uint64& seed) {
    seed += 0x9e3779b97f4a7c15ull;
    uint64 z = seed;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

// write random characters to the buffer
void randchars(void* buf, size_t bufsize);

// return a random string (length: @n), thread-safe
inline co::string randstr(uint32 n=15) {
    co::string res;
    if (n > 0) {
        res.resize(n);
        randchars(res.data(), res.size());
    }
    return res;
}

// return a random string (length: @n) with specific symbols, thread-safe.
// @charset is null-terminated, abbreviation like 0-9, a-f can be used.
co::string randstr(const char* charset, uint32 n);

} // co
