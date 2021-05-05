#pragma once

#include "def.h"
#include "hash/murmur_hash.h"
#include "hash/crc16.h"
#include "hash/md5.h"
#include "hash/base64.h"
#include "hash/url.h"

/**
 * 64 bit hash 
 *
 * @param s  a pointer to the data, it may not work on some systems if s is not 
 *           8-byte aligned.
 * @param n  size of the data.
 */
inline uint64 hash64(const void* s, size_t n) {
    return murmur_hash64(s, n, 0);
}

inline uint64 hash64(const char* s) {
    return hash64(s, strlen(s));
}

inline uint64 hash64(const fastring& s) {
    return hash64(s.data(), s.size());
}

inline uint64 hash64(const std::string& s) {
    return hash64(s.data(), s.size());
}

/**
 * 32 bit hash 
 *   - The result is the lower 32 bit of murmur_hash64 on 64 bit platforms. On 
 *     32 bit platforms, murmur_hash32 will be used instead. 
 * 
 * @param s  a pointer to the data, it may not work on some systems if s is not 
 *           4-byte or 8-byte aligned.
 * @param n  size of the data.
 */
inline uint32 hash32(const void* s, size_t n) {
    return (uint32) murmur_hash(s, n);
}

inline uint32 hash32(const char* s) {
    return hash32(s, strlen(s));
}

inline uint32 hash32(const fastring& s) {
    return hash32(s.data(), s.size());
}

inline uint32 hash32(const std::string& s) {
    return hash32(s.data(), s.size());
}
