#pragma once

#include "../fastring.h"

__coapi uint16_t crc16(const void* s, size_t n, uint16_t crc);

inline uint16_t crc16(const void* s, size_t n) {
    return crc16(s, n, 0);
}

inline uint16_t crc16(const char* s) {
    return crc16(s, strlen(s));
}

inline uint16_t crc16(const fastring& s) {
    return crc16(s.data(), s.size());
}

inline uint16_t crc16(const std::string& s) {
    return crc16(s.data(), s.size());
}
