/*
 * Murmurhash from http://sites.google.com/site/murmurhash.
 * Written by Austin Appleby, and is placed to the public domain.
 * For business purposes, Murmurhash is under the MIT license.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

uint32_t murmur_hash32(const void* data, size_t len, uint32_t seed);
uint64_t murmur_hash64(const void* data, size_t len, uint64_t seed);

template<int>
size_t murmur_hash(const void* s, size_t n);

template<>
inline size_t murmur_hash<4>(const void* s, size_t n) {
    return murmur_hash32(s, n, 0);
}

template<>
inline size_t murmur_hash<8>(const void* s, size_t n) {
    return murmur_hash64(s, n, 0);
}

inline size_t murmur_hash(const void* s, size_t n) {
    return murmur_hash<sizeof(size_t)>(s, n);
}
