/*
 * Murmurhash from http://sites.google.com/site/murmurhash.
 * Written by Austin Appleby, and is placed to the public domain.
 * For business purposes, Murmurhash is under the MIT license.
 */

#pragma once

#include "../def.h"
#include <stddef.h>
#include <string.h>

/**
 * 32 bit murmur hash 
 * 
 * @param s  a pointer to the data, it may not work on some systems if s is not 
 *           4-byte aligned.
 * @param n  size of the data.
 */
__codec uint32_t murmur_hash32(const void* s, size_t n, uint32_t seed);

/**
 * 64 bit murmur hash
 * 
 * @param s  a pointer to the data, it may not work on some systems if s is not 
 *           8-byte aligned.
 * @param n  size of the data.
 */
__codec uint64_t murmur_hash64(const void* s, size_t n, uint64_t seed);

namespace xx {
template<int>
size_t murmur_hash(const void* s, size_t n);

template<>
inline size_t murmur_hash<4>(const void* s, size_t n) {
    return (size_t) murmur_hash32(s, n, 0);
}

template<>
inline size_t murmur_hash<8>(const void* s, size_t n) {
    return (size_t) murmur_hash64(s, n, 0);
}
} // xx

/**
 * platform-specific murmur hash 
 *   - This function returns a hash value of type size_t, instead of uint32 or uint64, 
 *     which is to say, it is a 32 bit value on 32 bit platforms, or a 64 bit value on 
 *     64 bit platforms. 
 */
inline size_t murmur_hash(const void* s, size_t n) {
    return xx::murmur_hash<sizeof(size_t)>(s, n);
}
