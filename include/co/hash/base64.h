#pragma once

#include "../fastring.h"
#include <string>

/**
 * base64 encode 
 * 
 * @param s  a pointer to the data to be encoded.
 * @param n  size of the data.
 *
 * @return   a base64-encoded string.
 */
__codec fastring base64_encode(const void* s, size_t n);

/**
 * base64 decode 
 *   - This function will return an empty string in two cases:
 *     - Size of the input data is 0. 
 *     - The input data is not a valid base64-encoded string. 
 * 
 * @param s  a pointer to the data to be decoded.
 * @param n  size of the data.
 * 
 * @return   a decoded string on success, or an empty string on any error.
 */
__codec fastring base64_decode(const void* s, size_t n);

inline fastring base64_encode(const char* s) {
    return base64_encode(s, strlen(s));
}

inline fastring base64_encode(const fastring& s) {
    return base64_encode(s.data(), s.size());
}

inline fastring base64_encode(const std::string& s) {
    return base64_encode(s.data(), s.size());
}

inline fastring base64_decode(const char* s) {
    return base64_decode(s, strlen(s));
}

inline fastring base64_decode(const fastring& s) {
    return base64_decode(s.data(), s.size());
}

inline fastring base64_decode(const std::string& s) {
    return base64_decode(s.data(), s.size());
}
