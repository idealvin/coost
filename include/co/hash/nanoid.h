#pragma once

#include "../fastring.h"

/**
 * generate a nanoid with default symbols
 * 
 * @param n  length of the nanoid (>0), 15 by default
 */
__coapi fastring nanoid(int n=15);


/**
 * generate a nanoid
 * 
 * @param s    a string stores symbols to be used in the nanoid
 * @param len  length of the string, 2 <= len <= 255
 * @param n    length of the nanoid, n > 0
 */
__coapi fastring nanoid(const char* s, size_t len, int n);

inline fastring nanoid(const fastring& s, int n) {
    return nanoid(s.data(), s.size(), n);
}

inline fastring nanoid(const std::string& s, int n) {
    return nanoid(s.data(), s.size(), n);
}

inline fastring nanoid(const char* s, int n) {
    return nanoid(s, strlen(s), n);
}
