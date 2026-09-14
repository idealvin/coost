/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * See md5.c for more information.
 */
#pragma once

#include "string.h"

namespace co {

typedef struct {
    uint32 lo, hi;
    uint32 a, b, c, d;
    uint8 buffer[64];
    uint32 block[16];
} md5_ctx_t;

void md5_init(md5_ctx_t* ctx);
void md5_update(md5_ctx_t* ctx, const void* s, size_t n);
void md5_final(md5_ctx_t* ctx, uint8 res[16]);


// md5digest, 16-byte binary string
inline void md5digest(const void* s, size_t n, char res[16]) {
    md5_ctx_t ctx;
    md5_init(&ctx);
    md5_update(&ctx, s, n);
    md5_final(&ctx, (uint8*)res);
}

// return a 16-byte binary string
inline co::string md5digest(const void* s, size_t n) {
    co::string x;
    x.resize(16);
    md5digest(s, n, x.data());
    return x;
}

inline co::string md5digest(const char* s) {
    return md5digest(s, strlen(s));
}

inline co::string md5digest(const co::string& s) {
    return md5digest(s.data(), s.size());
}

inline co::string md5digest(const std::string& s) {
    return md5digest(s.data(), s.size());
}

// md5sum, result is stored in @res.
void md5sum(const void* s, size_t n, char res[32]);

// return a 32-byte string containing only hexadecimal digits
inline co::string md5sum(const void* s, size_t n) {
    co::string x;
    x.resize(32);
    md5sum(s, n, x.data());
    return x;
}

inline co::string md5sum(const char* s) {
    return md5sum(s, strlen(s));
}

inline co::string md5sum(const co::string& s) {
    return md5sum(s.data(), s.size());
}

inline co::string md5sum(const std::string& s) {
    return md5sum(s.data(), s.size());
}

} // co
