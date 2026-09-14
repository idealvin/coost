#include "co/base64.h"

namespace co {

static const char et[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

co::string base64_encode(const void* p, size_t n) {
    const size_t k = n / 3;
    const int r = (int) (n - k * 3); // r = n % 3

    co::string v;
    v.resize((k + !!r) * 4); // size: 4k or 4k + 4
    char* x = v.data();

    unsigned char a, b, c;
    const unsigned char* s = (const unsigned char*) p;
    const unsigned char* e = s + n - r;

    for (; s < e; s += 3, x += 4) {
        a = s[0];
        b = s[1];
        c = s[2];
        x[0] = et[a >> 2];
        x[1] = et[((a << 4) | (b >> 4)) & 0x3f];
        x[2] = et[((b << 2) | (c >> 6)) & 0x3f];
        x[3] = et[c & 0x3f];
    }

    switch (r) {
        case 1:
            c = s[0];
            x[0] = et[c >> 2];
            x[1] = et[(c & 0x03) << 4];
            x[2] = '=';
            x[3] = '=';
            break;
        case 2:
            a = s[0];
            b = s[1];
            x[0] = et[a >> 2];
            x[1] = et[((a & 0x03) << 4) | (b >> 4)];
            x[2] = et[(b & 0x0f) << 2];
            x[3] = '=';
            break;
    }

    return v;
}

static const int8 dt[256] = {
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

co::string base64_decode(const void* p, size_t n) {
    if (n < 4) return co::string();

    co::string v(n * 3 >> 2); // size <= n * 3/4
    char* x = v.data();
    int m = 0;
    const unsigned char* s = (const unsigned char*) p;
    const unsigned char* e = s + n;

    while (s + 8 <= e) {
        m = (dt[s[0]] << 18) | (dt[s[1]] << 12) | (dt[s[2]] << 6) | (dt[s[3]]);
        if (m < 0) goto err;

        x[0] = (char) ((m & 0x00ff0000) >> 16);
        x[1] = (char) ((m & 0x0000ff00) >> 8);
        x[2] = (char) (m & 0x000000ff);
        s += 4, x += 3;

        if (*s == '\r') {
            if (*++s != '\n') goto err;
            ++s;
        }
    }

    switch ((int)(e - s)) { /* 0 < e - s < 8 */
        case 4:
            break;
        case 6:
            if (e[-2] != '\r' && e[-1] != '\n') goto err;
            e -= 2;
            break;
        default:
            goto err;
    }

    do {
        m = (dt[s[0]] << 18) | (dt[s[1]] << 12);// (a | b);
        if (m < 0) goto err;
        *x++ = (char) ((m & 0x00ff0000) >> 16);

        if (s[2] == '=') {
            if (s[3] == '=') break;
            goto err;
        }

        m |= (dt[s[2]] << 6);
        if (m < 0) goto err;
        *x++ = (char) ((m & 0x0000ff00) >> 8);

        if (s[3] != '=') {
            m |= dt[s[3]];
            if (m < 0) goto err;
            *x++ = (char) (m & 0x000000ff);
        }
    } while (0);

    v.resize(x - v.data());
    return v;

err:
    return co::string();
}

} // co
