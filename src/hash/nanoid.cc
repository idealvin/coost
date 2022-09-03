// A thread-safe C++ implement for nanoid.
// It is inspired by github.com/mcmikecreations/nanoid_cpp.
// Also see https://github.com/ai/nanoid for details.
#include "co/hash/nanoid.h"
#include "co/god.h"
#include <math.h>
#include <random>

#ifdef _WIN32
#include <intrin.h>

inline uint32 _get_mask(uint32 x) { /* x > 1 */
    unsigned long r;
    _BitScanReverse(&r, x - 1);
    return (2u << r) - 1;
}

#else
inline uint32 _get_mask(uint32 x) { /* x > 1 */
    return (2u << (31 - __builtin_clz(x - 1))) - 1;
}
#endif

inline std::mt19937& _random() {
    static thread_local std::mt19937 r(std::random_device{}());
    return r;
}

inline void gen_random_bytes(uint8* p, uint32 n) {
    auto& random = _random();
    const uint32 x = (n >> 2) << 2;
    uint32 i = 0;
    for (; i < x; i += 4) {
        *(uint32*)(p + i) = random();
    }

    if (i < n) {
        *(uint32*)(p + i) = random();
    }
}

// 2 <= len <= 255, n > 0
fastring nanoid(const char* s, size_t len, int n) {
    if (unlikely(len < 2 || len > 255 || n <= 0)) return fastring();

    const uint32 L = static_cast<uint32>(len);
    const uint32 mask = _get_mask(L);
    const uint32 step = (uint32)::ceil(1.6 * (mask * n) / L);
    fastring bytes(god::align_up(step, 4));
    fastring res(n);
    int pos = 0;

    res.resize(n);
    while (true) {
        gen_random_bytes((uint8*)bytes.data(), step);
        for (size_t i = 0; i < step; ++i) {
            const size_t index = bytes[i] & mask;
            if (index < len) {
                res[pos] = s[index];
                if (++pos == n) return res;
            }
        }
    }
}

const char* const kSymbols = "_-0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

fastring nanoid(int n) {
    if (unlikely(n <= 0)) n = 15;
    const uint32 mask = 63;
    const uint32 p = mask * n;
    const uint32 r = p / 40;
    const uint32 step = r + !!(p - r * 40); // ceil(1.6 * mask * n / 64)
    fastring bytes(god::align_up(step, 4));
    fastring res(n);
    int pos = 0;

    res.resize(n);
    while (true) {
        gen_random_bytes((uint8*)bytes.data(), step);
        for (size_t i = 0; i < step; ++i) {
            const size_t index = bytes[i] & mask;
            res[pos] = kSymbols[index];
            if (++pos == n) return res;
        }
    }
}
