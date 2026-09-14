/*
 * Murmurhash written by Austin Appleby, and is placed to the public domain.
 * For business purposes, Murmurhash is under the MIT license.
 */

// Modified by Alvin at 2026/8/30, support unaligned addresses.

#include "co/murmur_hash.h"
#include <stdint.h>
#include <string.h>

namespace co {

// Loads a word from the given memory address.
// Using memcpy to safely read potentially unaligned bytes.
// Under optimization (-O2/-O3), modern compilers inline and convert this
// fixed-size copy into a single native load instruction (e.g., movq on x86-64).
inline size_t loadword(const void* p) {
    size_t r;
    memcpy(&r, p, sizeof(r));
    return r;
}

#if SIZE_MAX == UINT64_MAX
size_t murmur_hash(const void* key, size_t len) {
    const size_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;
    size_t h = (len * m); // seed ^ (len * m), seed is 0 here
    const unsigned char* p = static_cast<const unsigned char*>(key);
    const unsigned char* const e = p + (len & ~(size_t)7); // len / 8 * 8

    for (; p != e; p += 8) {
        size_t k = loadword(p);
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }

    switch (len & 7) {
        case 7:
            h ^= static_cast<size_t>(p[6]) << 48;
        case 6:
            h ^= static_cast<size_t>(p[5]) << 40;
        case 5:
            h ^= static_cast<size_t>(p[4]) << 32;
        case 4:
            h ^= static_cast<size_t>(p[3]) << 24;
        case 3:
            h ^= static_cast<size_t>(p[2]) << 16;
        case 2:
            h ^= static_cast<size_t>(p[1]) << 8;
        case 1:
            h ^= static_cast<size_t>(p[0]);
            h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}

#elif SIZE_MAX == UINT32_MAX
size_t murmur_hash(const void* key, size_t len) {
    const size_t m = 0x5bd1e995;
    const size_t r = 24;
    size_t h = len; // seed ^ len, seed is 0 here
    const unsigned char* p = static_cast<const unsigned char*>(key);
    const unsigned char* const e = p + (len & ~(size_t)3); // len / 4 * 4

    for (; p != e; p += 4) {
        size_t k = loadword(p);
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
    }

    switch (len & 3) {
        case 3:
            h ^= static_cast<size_t>(p[2]) << 16;
        case 2:
            h ^= static_cast<size_t>(p[1]) << 8;
        case 1:
            h ^= static_cast<size_t>(p[0]);
            h *= m;
    };

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

#else
#error "Unsupported platform: size_t must be 32-bit or 64-bit."
#endif

} // co
