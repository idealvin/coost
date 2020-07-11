#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

uint16_t crc16(const void* s, size_t n, uint16_t crc);

inline uint16_t crc16(const void* s, size_t n) {
    return crc16(s, n, 0);
}

inline uint16_t crc16(const char* s) {
    return crc16(s, strlen(s));
}

template<typename S>
inline uint16_t crc16(const S& s) {
    return crc16(s.data(), s.size());
}
