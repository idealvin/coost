/**
 * Sha256.h -- SHA-256 Hash
 * 2010-06-11 : Igor Pavlov : Public domain
 * 2022-05-23 : Modified by Alvin.
 */
#pragma once

#include "../fastring.h"

typedef struct {
    uint32 state[8];
    uint64 count;
    uint8 buffer[64];
} sha256_ctx_t;

__coapi void sha256_init(sha256_ctx_t* ctx);
__coapi void sha256_update(sha256_ctx_t* ctx, const void* s, size_t n);
__coapi void sha256_final(sha256_ctx_t* ctx, uint8 res[32]);


// sha256digest, 32-byte binary string
inline void sha256digest(const void* s, size_t n, char res[32]) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, s, n);
    sha256_final(&ctx, (uint8*)res);
}

// return a 32-byte binary string
inline fastring sha256digest(const void* s, size_t n) {
    fastring x(32);
    x.resize(32);
    sha256digest(s, n, &x[0]);
    return x;
}

inline fastring sha256digest(const char* s) {
    return sha256digest(s, strlen(s));
}

inline fastring sha256digest(const fastring& s) {
    return sha256digest(s.data(), s.size());
}

inline fastring sha256digest(const std::string& s) {
    return sha256digest(s.data(), s.size());
}


// sha256sum, result is stored in @res.
__coapi void sha256sum(const void* s, size_t n, char res[64]);

// return a 64-byte string containing only hexadecimal digits.
inline fastring sha256sum(const void* s, size_t n) {
    fastring x(64);
    x.resize(64);
    sha256sum(s, n, &x[0]);
    return x;
}

inline fastring sha256sum(const char* s) {
    return sha256sum(s, strlen(s));
}

inline fastring sha256sum(const fastring& s) {
    return sha256sum(s.data(), s.size());
}

inline fastring sha256sum(const std::string& s) {
    return sha256sum(s.data(), s.size());
}
