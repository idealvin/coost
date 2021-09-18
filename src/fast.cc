#include "co/fast.h"
#include <string.h>

namespace fast {

static void init_itoh_table(uint16* p) {
    for (int i = 0; i < 256; ++i) {
        char* b = (char*)(p + i);
        b[0] = "0123456789abcdef"[i >> 4];
        b[1] = "0123456789abcdef"[i & 0x0f];
    }    
}

static void init_itoa_table(uint32* p) {
    for (int i = 0; i < 10000; ++i) {
        char* b = (char*)(p + i);
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

inline uint16* create_itoh_table() {
    static uint16 itoh_table[256];
    init_itoh_table(itoh_table);
    return itoh_table;
}

inline uint32* create_itoa_table() {
    static uint32 itoa_table[10000];
    init_itoa_table(itoa_table);
    return itoa_table;
}

inline uint16* get_itoh_table() {
    static uint16* itoh_table = create_itoh_table();
    return itoh_table;
}

inline uint32* get_itoa_table() {
    static uint32* itoa_table = create_itoa_table();
    return itoa_table;
}

int u32toh(uint32 v, char* buf) {
    static uint16* itoh_table = get_itoh_table();
    uint16 b[4], *p = b + 4;

    do {
        *--p = itoh_table[v & 0xff];
        v >>= 8;
    } while (v > 0);

    buf[0] = '0';
    buf[1] = 'x';
    int len = (int) ((char*)(b + 4) - (char*)p - (*(char*)p == '0'));
    memcpy(buf + 2, (char*)(b + 4) - len, (size_t)len);
    return len + 2;
}

int u64toh(uint64 v, char* buf) {
    static uint16* itoh_table = get_itoh_table();
    uint16 b[8], *p = b + 8;

    do {
        *--p = itoh_table[v & 0xff];
        v >>= 8;
    } while (v > 0);

    buf[0] = '0';
    buf[1] = 'x';
    int len = (int) ((char*)(b + 8) - (char*)p - (*(char*)p == '0'));
    memcpy(buf + 2, (char*)(b + 8) - len, (size_t)len);
    return len + 2;
}

int u32toa(uint32 v, char* buf) {
    static uint32* itoa_table = get_itoa_table();
    uint32 b[3], *p = b + 2;

    if (v > 9999) {
        b[1] = v / 10000;
        b[2] = itoa_table[v - b[1] * 10000] | 0x30303030;
        --p;
    } else {
        b[2] = itoa_table[v];
        goto u32toa_end;
    }

    if (b[1] > 9999) {
        b[0] = b[1] / 10000;
        b[1] = itoa_table[b[1] - b[0] * 10000] | 0x30303030;
        b[0] = itoa_table[b[0]];
        --p;
    } else {
        b[1] = itoa_table[b[1]];
        goto u32toa_end;
    }

  u32toa_end:
    int len = (int) (((char*)b) + 9 + ((*(char*)p) >> 4) - ((char*)p));
    memcpy(buf, ((char*)b) + 12 - len, (size_t)len);
    return len;
}

int u64toa(uint64 v, char* buf) {
    static uint32* itoa_table = get_itoa_table();
    uint32 b[5], *p = b + 4;
    uint64 x;

    if (v > 9999) {
        x = v / 10000;
        b[4] = itoa_table[v - x * 10000] | 0x30303030;
        --p;
    } else {
        b[4] = itoa_table[v];
        goto u64toa_end;
    }

    if (x > 9999) {
        v = x / 10000;
        b[3] = itoa_table[x - v * 10000] | 0x30303030;
        --p;
    } else {
        b[3] = itoa_table[x];
        goto u64toa_end;
    }

    if (v > 9999) {
        x = v / 10000;
        b[2] = itoa_table[v - x * 10000] | 0x30303030;
        --p;
    } else {
        b[2] = itoa_table[v];
        goto u64toa_end;
    }

    if (x > 9999) {
        b[0] = (uint32) (x / 10000);
        b[1] = itoa_table[x - b[0] * 10000] | 0x30303030;
        b[0] = itoa_table[b[0]];
        --p;
    } else {
        b[1] = itoa_table[x];
        goto u64toa_end;
    }

  u64toa_end:
    int len = (int) (((char*)b) + 17 + ((*(char*)p) >> 4) - ((char*)p));
    memcpy(buf, ((char*)b) + 20 - len, (size_t)len);
    return len;
}

} // namespace fast
