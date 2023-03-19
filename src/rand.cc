#include "co/rand.h"
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

namespace co {

class Rand {
  public:
    Rand() : _mt(std::random_device{}()) {
        const uint32 seed = _mt();
        _seed = (0 < seed && seed < 2147483647u) ? seed : 23u;
    }

    // _seed = _seed * A % M
    uint32 next() {
        static const uint32 M = 2147483647u;  // 2^31-1
        static const uint64 A = 16385;        // 2^14+1
        const uint64 p = _seed * A;
        _seed = static_cast<uint32>((p >> 31) + (p & M));
        return _seed > M ? (_seed -= M) : _seed;
    }

    std::mt19937& mt19937() { return _mt; }

  private:
    std::mt19937 _mt;
    uint32 _seed;
};

inline Rand& _rand() {
    static thread_local Rand r;
    return r;
}

uint32 rand() {
    return _rand().next();
}

inline void _gen_random_bytes(uint8* p, uint32 n) {
    auto& r = _rand().mt19937();
    const uint32 x = (n >> 2) << 2;
    uint32 i = 0;
    for (; i < x; i += 4) *(uint32*)(p + i) = r();
    i < n ? (void)(*(uint32*)(p + i) = r()) : (void)0;
}

const char kS[] = "_-0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

// A thread-safe C++ implement for nanoid
// Inspired by github.com/mcmikecreations/nanoid_cpp.
// Also see https://github.com/ai/nanoid for details.
fastring randstr(int n) {
    n <= 0 ? (void)(n = 15) : (void)0;
    const uint32 mask = 63;
    const uint32 p = mask * n;
    const uint32 r = p / 40;
    const uint32 step = r + !!(p - r * 40); // ceil(1.6 * mask * n / 64)
    fastring bytes(god::align_up<4>(step));
    fastring res(n);
    int pos = 0;

    res.resize(n);
    while (true) {
        _gen_random_bytes((uint8*)bytes.data(), step);
        for (size_t i = 0; i < step; ++i) {
            const size_t index = bytes[i] & mask;
            res[pos] = kS[index];
            if (++pos == n) return res;
        }
    }
}

// 2 <= len <= 255, n > 0
fastring randstr(const char* s, int n) {
    const uint32 len = (uint32) strlen(s);
    if (unlikely(len < 2 || len > 255)) return fastring();
    n <= 0 ? (void)(n = 15) : (void)0;

    const uint32 mask = _get_mask(len);
    const uint32 step = (uint32)::ceil(1.6 * (mask * n) / len);
    fastring bytes(god::align_up<4>(step));
    fastring res(n);
    int pos = 0;

    res.resize(n);
    while (true) {
        _gen_random_bytes((uint8*)bytes.data(), step);
        for (size_t i = 0; i < step; ++i) {
            const size_t index = bytes[i] & mask;
            if (index < len) {
                res[pos] = s[index];
                if (++pos == n) return res;
            }
        }
    }
}

} // co
