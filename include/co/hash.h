#pragma once

#include "def.h"
#include "hash/murmur_hash.h"
#include "hash/crc16.h"
#include "hash/md5.h"
#include "hash/base64.h"

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
#else
// use murmur_hash32 on 32 bit platform
inline uint32 hash32(const void* s, size_t n) {
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
