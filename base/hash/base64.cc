#include "base64.h"

static const char* entab =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char detab[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

fastring base64_encode(const void* p, size_t n) {
    size_t k = n / 3;
    int r = (int) (n - k * 3); // r = n % 3

    fastring v;
    v.resize((k + !!r) * 4);   // size: 4k or 4k + 4
    char* x = (char*) v.data();

    unsigned char a, b, c;
    const unsigned char* s = (const unsigned char*) p;

    for (size_t i = 0; i < k; ++i) {
        a = *s++;
        b = *s++;
        c = *s++;
        *x++ = entab[a >> 2];
        *x++ = entab[((a << 4) | (b >> 4)) & 0x3f];
        *x++ = entab[((b << 2) | (c >> 6)) & 0x3f];
        *x++ = entab[c & 0x3f];
    }

    if (r == 1) {
        c = *s++;
        *x++ = entab[c >> 2];
        *x++ = entab[(c & 0x03) << 4];
        *x++ = '=';
        *x++ = '=';
    } else if (r == 2) {
        a = *s++;
        b = *s++;
        *x++ = entab[a >> 2];
        *x++ = entab[((a & 0x03) << 4) | (b >> 4)];
        *x++ = entab[(b & 0x0f) << 2];
        *x++ = '=';
    }

    return v;
}

fastring base64_decode(const void* p, size_t n) {
    fastring v(n * 3 >> 2); // size <= n * 3/4
    char* x = (char*) v.data();

    int m = 0;
    const unsigned char* s = (const unsigned char*) p;

    while (n >= 8) {
        m = detab[*s++] << 18;
        m |= detab[*s++] << 12;
        m |= detab[*s++] << 6;
        m |= detab[*s++];
        if (unlikely(m < 0)) goto err;

        *x++ = (char) ((m & 0x00ff0000) >> 16);
        *x++ = (char) ((m & 0x0000ff00) >> 8);
        *x++ = (char) (m & 0x000000ff);
        n -= 4;

        if (*s == '\r' || *s == '\n') {
            --n, ++s;
            if (*s == '\n' || *s == '\r') { --n, ++s; };
        }
    }

    if (unlikely(s[n - 1] == '\n' || s[n - 1] == '\r')) {
        --n;
        if (s[n - 1] == '\r' || s[n - 1] == '\n') --n;
    }

    if (unlikely(n != 4)) goto err;

    do {
        m = detab[*s++] << 18;
        m |= detab[*s++] << 12;
        if (unlikely(m < 0)) goto err;
        *x++ = (char) ((m & 0x00ff0000) >> 16);

        if (*s == '=') {
            if (s[1] == '=') break;
            goto err;
        }

        m |= detab[*s++] << 6;
        if (unlikely(m < 0)) goto err;
        *x++ = (char) ((m & 0x0000ff00) >> 8);

        if (*s != '=') {
            m |= detab[*s++];
            if (unlikely(m < 0)) goto err;
            *x++ = (char) (m & 0x000000ff);
        }
    } while (0);

    v.resize(x - v.data());
    return v;

  err:
    throw fastring("base64 decode error: ").append(p, n);
}
