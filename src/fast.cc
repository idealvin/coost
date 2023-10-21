#include "co/fast.h"
#include <string.h>

namespace fast {
namespace xx {

static int g_nifty_counter;
static uint16 g_itoh_tb[256];
static uint32 g_itoa_tb[10000];

Initializer::Initializer() {
    if (g_nifty_counter++ == 0) {
        for (int i = 0; i < 256; ++i) {
            char* b = (char*)(g_itoh_tb + i);
            b[0] = "0123456789abcdef"[i >> 4];
            b[1] = "0123456789abcdef"[i & 0x0f];
        }    

        for (int i = 0; i < 10000; ++i) {
            char* b = (char*)(g_itoa_tb + i);
            b[3] = (char)(i % 10 + '0');
            b[2] = (char)(i % 100 / 10 + '0');
            b[1] = (char)(i % 1000 / 100 + '0');
            b[0] = (char)(i / 1000);

            // digits of i: (b[0] >> 4) + 1 
            if (i > 999) {
                b[0] |= (3 << 4); // 0x30
            } else if (i > 99) {
                b[0] |= (2 << 4); // 0x20
            } else if (i > 9) {
                b[0] |= (1 << 4); // 0x10
            }
        }
    }
}

} // xx

using xx::g_itoh_tb;
using xx::g_itoa_tb;

int u32toh(uint32 v, char* buf) {
    uint16 b[4], *p = b + 4;

    do {
        *--p = g_itoh_tb[v & 0xff];
        v >>= 8;
    } while (v > 0);

    buf[0] = '0';
    buf[1] = 'x';
    int len = (int) ((char*)(b + 4) - (char*)p - (*(char*)p == '0'));
    memcpy(buf + 2, (char*)(b + 4) - len, (size_t)len);
    return len + 2;
}

int u64toh(uint64 v, char* buf) {
    uint16 b[8], *p = b + 8;

    do {
        *--p = g_itoh_tb[v & 0xff];
        v >>= 8;
    } while (v > 0);

    buf[0] = '0';
    buf[1] = 'x';
    int len = (int) ((char*)(b + 8) - (char*)p - (*(char*)p == '0'));
    memcpy(buf + 2, (char*)(b + 8) - len, (size_t)len);
    return len + 2;
}

int u32toa(uint32 v, char* buf) {
    uint32 b[3], *p = b + 2;

    if (v > 9999) {
        b[1] = v / 10000;
        b[2] = g_itoa_tb[v - b[1] * 10000] | 0x30303030;
        --p;
    } else {
        b[2] = g_itoa_tb[v];
        goto u32toa_end;
    }

    if (b[1] > 9999) {
        b[0] = b[1] / 10000;
        b[1] = g_itoa_tb[b[1] - b[0] * 10000] | 0x30303030;
        b[0] = g_itoa_tb[b[0]];
        --p;
    } else {
        b[1] = g_itoa_tb[b[1]];
        goto u32toa_end;
    }

  u32toa_end:
    int len = (int) (((char*)b) + 9 + ((*(char*)p) >> 4) - ((char*)p));
    memcpy(buf, ((char*)b) + 12 - len, (size_t)len);
    return len;
}

int u64toa(uint64 v, char* buf) {
    uint32 b[5], *p = b + 4;
    uint64 x;

    if (v > 9999) {
        x = v / 10000;
        b[4] = g_itoa_tb[v - x * 10000] | 0x30303030;
        --p;
    } else {
        b[4] = g_itoa_tb[v];
        goto u64toa_end;
    }

    if (x > 9999) {
        v = x / 10000;
        b[3] = g_itoa_tb[x - v * 10000] | 0x30303030;
        --p;
    } else {
        b[3] = g_itoa_tb[x];
        goto u64toa_end;
    }

    if (v > 9999) {
        x = v / 10000;
        b[2] = g_itoa_tb[v - x * 10000] | 0x30303030;
        --p;
    } else {
        b[2] = g_itoa_tb[v];
        goto u64toa_end;
    }

    if (x > 9999) {
        b[0] = (uint32) (x / 10000);
        b[1] = g_itoa_tb[x - b[0] * 10000] | 0x30303030;
        b[0] = g_itoa_tb[b[0]];
        --p;
    } else {
        b[1] = g_itoa_tb[x];
        goto u64toa_end;
    }

  u64toa_end:
    int len = (int) (((char*)b) + 17 + ((*(char*)p) >> 4) - ((char*)p));
    memcpy(buf, ((char*)b) + 20 - len, (size_t)len);
    return len;
}

} // namespace fast
