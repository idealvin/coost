#include "co/rand.h"
#include "co/align.h"
#include "co/thread.h"
#include <chrono>
#include <random>
#ifdef _WIN32
#include <intrin.h>
#endif

namespace co {

constexpr uint32 M = 0x7fffffffu;  // 2^31-1
constexpr uint64 A = 16385;

struct __cacheline_aligned Rand {
    Rand();

    // _seed = _seed * A % M
    uint32 next() {
        const uint64 p = _seed * A;
        _seed = static_cast<uint32>((p >> 31) + (p & M));
        return _seed < M ? _seed : (_seed -= M);
    }

    uint64 next64() {
        _seed64 += 0x9e3779b97f4a7c15ull;
        uint64 z = _seed64;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }

    struct Cache {
        constexpr Cache() : s(), p(0), plen(0) {}
        co::string s;
        const char* p;
        uint32 plen;
    };

    uint32 _seed;
    uint64 _seed64;
    Cache _cache;
};

Rand::Rand() {
    const uint32 t = (uint32)(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::random_device rd;
    std::seed_seq seed { rd(), rd(), t, co::thread_id() };
    std::mt19937 g(seed);

    do {
        _seed = g() & M;
    } while (_seed == 0 || _seed == M);

    const uint32 h = g();
    const uint32 l = g();
    _seed64 = (static_cast<uint64>(h) << 32) | static_cast<uint64>(l);
}

static thread_local Rand g_rand;

uint32 rand() { return g_rand.next(); }
uint64 rand64() { return g_rand.next64(); }

__cacheline_aligned static const char g_s[] =
    "abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ-0123456789";
static const char* const paz = g_s;
static const char* const pAZ = g_s + 27;
static const char* const p09 = g_s + 54;

void randchars(void* buf, size_t bufsize) {
    constexpr uint32 mask = 63;
    const char* const charset = g_s;
    auto& random = g_rand;
    char* dst = (char*)buf;
    char* const end = dst + co::align_down<8>(bufsize);

    while (dst < end) {
        uint64 x = random.next64();
        uint8* const p = (uint8*)&x;
        dst[0] = charset[p[0] & mask];
        dst[1] = charset[p[1] & mask];
        dst[2] = charset[p[2] & mask];
        dst[3] = charset[p[3] & mask];
        dst[4] = charset[p[4] & mask];
        dst[5] = charset[p[5] & mask];
        dst[6] = charset[p[6] & mask];
        dst[7] = charset[p[7] & mask];
        dst += 8;
    }

    const uint32 r = static_cast<uint32>(bufsize & 7);
    if (r > 0) {
        uint64 x = random.next64();
        uint8* const p = (uint8*)&x;
        for (uint32 i = 0; i < r; ++i) {
            dst[i] = charset[p[i] & mask];
        }
    }
}

static uint32 _expand(const char** charset) {
    auto& cache = g_rand._cache;
    auto& s = cache.s;
    const char* p = *charset;
    if (p == cache.p) {
        if (!s.empty()) {
            *charset = s.data();
            return (uint32) s.size();
        }
        return cache.plen;
    }

    const size_t n = strlen(p);
    runtime_assert(n < 256);

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
        s.clear();
        cache.p = p;
        cache.plen = (uint32)n;
        return (uint32)n;
    }

    cache.p = p;
    s.append(p + x, n - x);
    *charset = s.data();
    return (uint32)s.size();
}

// power 2 aligned - 1
#ifdef _WIN32
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

co::string randstr(const char* charset, uint32 n) {
    if (__unlikely(!charset || !*charset || n == 0)) return co::string();

    const uint32 len = _expand(&charset);
    runtime_assert(len < 256);
    if (__unlikely(len == 1)) return co::string(n, *charset);

    const uint32 mask = _get_mask(len);
    auto& random = g_rand;
    co::string res;
    res.resize(n);

    char* dst = (char*)res.data();
    char* const end = dst + n;
    while (dst < end) {
        uint64 x = random.next64();
        uint8* const p = (uint8*)&x;
        for (int i = 0; i < 8 && dst < end; ++i) {
            const uint32 idx = p[i] & mask;
            if (idx < len) *dst++ = charset[idx];
        }
    }
    return res;
}

} // co
