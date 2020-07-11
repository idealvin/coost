/*
 * Murmurhash from http://sites.google.com/site/murmurhash.
 * Written by Austin Appleby, and is placed to the public domain.
 * For business purposes, Murmurhash is under the MIT license.
 */

#pragma once

#include "../def.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

uint32_t murmur_hash32(const void* data, size_t len, uint32_t seed);
uint64_t murmur_hash64(const void* data, size_t len, uint64_t seed);

// murmur2 64 bit hash
inline uint64 hash64(const void* s, size_t n) {
    return murmur_hash64(s, n, 0);
}

inline uint64 hash64(const char* s) {
    return hash64(s, strlen(s));
}

template<typename S>
inline uint64 hash64(const S& s) {
    return hash64(s.data(), s.size());
}

#ifdef ARCH64
// use the lower 32 bit of murmur_hash64 on 64 bit platform
inline uint32 hash32(const void* s, size_t n) {
    return (uint32) hash64(s, n);
}

inline size_t murmur_hash(const void* s, size_t n) {
    return murmur_hash64(s, n, 0);
}
#else
// use murmur_hash32 on 32 bit platform
inline uint32 hash32(const void* s, size_t n) {
    return murmur_hash32(s, n, 0);
}

inline size_t murmur_hash(const void* s, size_t n) {
    return murmur_hash32(s, n, 0);
}
#endif

inline uint32 hash32(const char* s) {
    return hash32(s, strlen(s));
}

template<typename S>
inline uint32 hash32(const S& s) {
    return hash32(s.data(), s.size());
}

inline size_t murmur_hash(const char* s) {
    return murmur_hash(s, strlen(s));
}

template<typename S>
inline size_t murmur_hash(const S& s) {
    return murmur_hash(s.data(), s.size());
}
