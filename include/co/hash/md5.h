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

#include "../fastring.h"
#include <string>

typedef struct {
    uint32 lo, hi;
    uint32 a, b, c, d;
    uint8 buffer[64];
    uint32 block[16];
} md5_ctx_t;

__codec void md5_init(md5_ctx_t* ctx);
__codec void md5_update(md5_ctx_t* ctx, const void* s, size_t n);
__codec void md5_finish(md5_ctx_t* ctx, uint8* result);

/**
 * @param s  a pointer to the input data.
 * @param n  size of the input data.
 * 
 * @return   a 32-byte string containing only hexadecimal digits.
 */
__codec fastring md5sum(const void* s, size_t n);

inline fastring md5sum(const char* s) {
    return md5sum(s, strlen(s));
}

inline fastring md5sum(const fastring& s) {
    return md5sum(s.data(), s.size());
}

inline fastring md5sum(const std::string& s) {
    return md5sum(s.data(), s.size());
}
