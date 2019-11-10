#pragma once

#include "hash/md5.h"
#include "hash/crc16.h"
#include "hash/base64.h"
#include "hash/murmur_hash.h"

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

// use the lower 32 bit of murmur2 hash64
inline uint32 hash32(const void* s, size_t n) {
    return (uint32) hash64(s, n);
}

inline uint32 hash32(const char* s) {
    return hash32(s, strlen(s));
}

template<typename S>
inline uint32 hash32(const S& s) {
    return hash32(s.data(), s.size());
}

inline uint16 crc16(const void* s, size_t n) {
    return crc16(s, n, 0);
}

inline uint16 crc16(const char* s) {
    return crc16(s, strlen(s));
}

template<typename S>
inline uint16 crc16(const S& s) {
    return crc16(s.data(), s.size());
}
