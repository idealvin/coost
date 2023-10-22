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

    struct Cache {
        constexpr Cache() : s(), p(0) {}
        fastring s;
        const char* p;
    };

    Cache& cache() { return _cache; }

  private:
    std::mt19937 _mt;
    uint32 _seed;
    Cache _cache;
};

static thread_local Rand g_rand;

uint32 rand() {
    return g_rand.next();
}

inline void _gen_random_bytes(uint8* p, uint32 n) {
    auto& r = g_rand.mt19937();
    const uint32 x = (n >> 2) << 2;
    uint32 i = 0;
    for (; i < x; i += 4) *(uint32*)(p + i) = r();
    if (i < n) *(uint32*)(p + i) = r();
}

const char kS[] = "_-0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char* const p09 = kS + 2;
const char* const paz = kS + 12;
const char* const pAZ = kS + 38;

// A thread-safe C++ implement for nanoid
// Inspired by github.com/mcmikecreations/nanoid_cpp.
// Also see https://github.com/ai/nanoid for details.
fastring randstr(int n) {
    if (unlikely(n <= 0)) return fastring();

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

const char* _expand(const char* p, uint32& len) {
    auto& cache = g_rand.cache();
    auto& s = cache.s;
    if (p == cache.p) {
        len = (uint32) s.size();
        return s.data();
    }

    const size_t n = strlen(p);
    if (n > 255) return nullptr;

    int m = 0;
    size_t x = 0;
    const char* q;
    for (size_t i = 1; i < n - 1;) {
        if (p[i] != '-') { ++i; continue; }

        const char a = p[i - 1];
        const char b = p[i + 1];
        if (a > b) goto _2;

        if ('0' <= a && b <= '9') { q = p09 + (a - '0'); goto _3; }
        if ('a' <= a && b <= 'z') { q = paz + (a - 'a'); goto _3; }
        if ('A' <= a && b <= 'Z') { q = pAZ + (a - 'A'); goto _3; }

      _2:
        i += 2;
        continue;

      _3:
        if (++m == 1) s.clear();
        s.append(p + x, i - 1 - x);
        s.append(q, b - a + 1);
        x = i + 2;
        i += 3;
        continue;
    }

    if (x == 0) {
        len = (uint32)n;
        return p;
    }

    cache.p = p;
    s.append(p + x, n - x);
    len = (uint32)s.size();
    return s.data();
}

fastring randstr(const char* s, int n) {
    if (!s || !*s || n <= 0) return fastring();

    uint32 len = 0;
    const char* p = _expand(s, len);
    if (!p || len == 0 || len > 255) return fastring();
    if (len == 1) return fastring(n, *p);

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
                res[pos] = p[index];
                if (++pos == n) return res;
            }
        }
    }
}

} // co
