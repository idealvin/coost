#pragma once

#include "../def.h"
#include "../fastring.h"

fastring base64_encode(const void* s, size_t n);

inline fastring base64_encode(const char* s) {
    return base64_encode(s, strlen(s));
}

template<typename S>
inline fastring base64_encode(const S& s) {
    return base64_encode(s.data(), s.size());
}

fastring base64_decode(const void* s, size_t n);

inline fastring base64_decode(const char* s) {
    return base64_decode(s, strlen(s));
}

template<typename S>
inline fastring base64_decode(const S& s) {
    return base64_decode(s.data(), s.size());
}
