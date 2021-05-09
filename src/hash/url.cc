#include "co/hash/url.h"

static const char* init_url_encode_table() {
    static char tb[256] = { 0 };
    const unsigned char* p = (const unsigned char*) "-_.~!*'();:@&=+$,/?#[]";
    const size_t n = strlen((const char*)p);
    for (size_t i = 0; i < n; ++i) tb[p[i]] = 1;
    for (int i = 'A'; i <= 'Z'; ++i) tb[i] = 1;
    for (int i = 'a'; i <= 'z'; ++i) tb[i] = 1;
    for (int i = '0'; i <= '9'; ++i) tb[i] = 1;
    return tb;
}

static inline bool unencoded(uint8 c) {
    static const char* tb = init_url_encode_table();
    return tb[c];
}

static inline int hex2int(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    return -1;
}

fastring url_encode(const void* s, size_t n) {
    fastring dst(n + 32);
    const char* p = (const char*)s;

    char c;
    for (size_t i = 0; i < n; ++i) {
        c = p[i];
        if (unencoded(c)) {
            dst.append(c);
            continue;
        }
        dst.append('%');
        dst.append("0123456789ABCDEF"[static_cast<uint8>(c) >> 4]);
        dst.append("0123456789ABCDEF"[static_cast<uint8>(c) & 0x0F]);
    }

    return dst;
}

fastring url_decode(const void* s, size_t n) {
    fastring dst(n);
    const char* p = (const char*)s;

    char c;
    for (size_t i = 0; i < n; ++i) {
        c = p[i];
        if (c != '%') {
            dst.append(c);
            continue;
        }

        if (i + 2 >= n) return fastring();       // invalid encode
        const int h4 = hex2int(p[i + 1]);
        const int l4 = hex2int(p[i + 2]);
        if (h4 < 0 || l4 < 0) return fastring(); // invalid encode

        dst.append((char)((h4 << 4) | l4));
        i += 2;
    }

    return dst;
}
