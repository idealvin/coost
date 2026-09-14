#pragma once

#include "string.h"

namespace co {

co::string base64_encode(const void* s, size_t n);

// return empty string on any error 
co::string base64_decode(const void* s, size_t n);

inline co::string base64_encode(const char* s) {
    return base64_encode(s, strlen(s));
}

inline co::string base64_encode(const co::string& s) {
    return base64_encode(s.data(), s.size());
}

inline co::string base64_encode(const std::string& s) {
    return base64_encode(s.data(), s.size());
}

inline co::string base64_decode(const char* s) {
    return base64_decode(s, strlen(s));
}

inline co::string base64_decode(const co::string& s) {
    return base64_decode(s.data(), s.size());
}

inline co::string base64_decode(const std::string& s) {
    return base64_decode(s.data(), s.size());
}

} // co
