#include "co/unitest.h"
#include "co/mem.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>
#endif


namespace test {
namespace mem {

#ifdef _WIN32
#if __arch64
inline int _find_msb(size_t x) { /* x != 0 */
    unsigned long i;
    _BitScanReverse64(&i, x);
    return (int)i;
}

inline uint32 _find_lsb(size_t x) { /* x != 0 */
    unsigned long r;
    _BitScanForward64(&r, x);
    return r;
}

#else
inline int _find_msb(size_t x) { /* x != 0 */
    unsigned long i;
    _BitScanReverse(&i, x);
    return (int)i;
}

inline uint32 _find_lsb(size_t x) { /* x != 0 */
    unsigned long r;
    _BitScanForward(&r, x);
    return r;
}
#endif

inline uint32 _pow2_align(uint32 n) {
    unsigned long r;
    _BitScanReverse(&r, n - 1);
    return 2u << r;
}

#else
#if __arch64
inline int _find_msb(size_t x) { /* x != 0 */
    return 63 - __builtin_clzll(x);
}

inline uint32 _find_lsb(size_t x) { /* x != 0 */
    return __builtin_ffsll(x) - 1;
}

#else
inline int _find_msb(size_t v) { /* x != 0 */
    return 31 - __builtin_clz(v);
}

inline uint32 _find_lsb(size_t x) { /* x != 0 */
    return __builtin_ffs(x) - 1;
}
#endif

inline uint32 _pow2_align(uint32 n) {
    return 1u << (32 - __builtin_clz(n - 1));
}

#endif

class MemBlocks {
  public:
    explicit MemBlocks(uint32 blk_size)
        : _m(0), _p(0), _blk_size(blk_size) {
    }

    ~MemBlocks() {
        if (_u) {
            for (uint32 i = 0; i < _u[-2]; ++i) ::free(_m[i]);
            ::free(_u - 2);
        }
    }

    void* alloc(uint32 n, uint32 align=sizeof(void*));
    uint32 size() const { return _u ? _u[-2] : 0; }
    uint32 blk_size() const { return _blk_size; }
    uint32 pos() const { return _p; }

  private:
    union {
        char** _m;
        uint32* _u;
    };
    uint32 _p;
    const uint32 _blk_size;
};

void* MemBlocks::alloc(uint32 n, uint32 align) {
    if (unlikely(_m == 0)) {
        _u = (uint32*)::malloc(sizeof(char*) * 7 + 8) + 2;
        _m[0] = (char*)::malloc(_blk_size);
        _u[-1] = 7; // cap
        _u[-2] = 1; // size
    }

    char* x = _m[_u[-2] - 1];
    char* p = align != sizeof(void*) ? god::align_up(x + _p, align) : x + _p;
    n = god::align_up(n, align);
    if (unlikely(p + n > x + _blk_size)) {
        if (_u[-2] == _u[-1]) {
            _u = (uint32*)::realloc(_u - 2, sizeof(char*) * _u[-1] * 2 + 8) + 2;
            _u[-1] *= 2;
        }
        x = (char*)::malloc(_blk_size);
        _m[_u[-2]] = x;
        _u[-2] += 1;
        _p = 0;
        p = align != sizeof(void*) ? god::align_up(x, align) : x;
    }

    _p = god::cast<uint32>(p - x) + n;
    return p;
}

} // mem

DEF_test(mem) {
    DEF_case(bitops) {
        EXPECT_EQ(mem::_find_lsb(1), 0);
        EXPECT_EQ(mem::_find_lsb(12), 2);
        EXPECT_EQ(mem::_find_lsb(3u << 20), 20);
      #if __arch64
        EXPECT_EQ(mem::_find_msb(1), 0);
        EXPECT_EQ(mem::_find_msb(12), 3);
        EXPECT_EQ(mem::_find_msb(3u << 20), 21);
        EXPECT_EQ(mem::_find_msb(1ull << 63), 63);
        EXPECT_EQ(mem::_find_msb(~0ull), 63);
        EXPECT_EQ(mem::_find_lsb(~0ull), 0);
      #else
        EXPECT_EQ(mem::_find_msb(1), 0);
        EXPECT_EQ(mem::_find_msb(12), 3);
        EXPECT_EQ(mem::_find_msb(3u << 20), 21);
        EXPECT_EQ(mem::_find_msb(1ull << 31), 31);
        EXPECT_EQ(mem::_find_msb(~0u), 31);
        EXPECT_EQ(mem::_find_lsb(~0u), 0);
      #endif
    }

    DEF_case(mb) {
        mem::MemBlocks mb(8 * 1024);
        char* p = (char*) mb.alloc(15);
        EXPECT(p);
        EXPECT_EQ(mb.pos(), 16);

        char* q = (char*) mb.alloc(63, 64);
        EXPECT_EQ((size_t)q & 63, 0);
        EXPECT_EQ(mb.pos(), god::cast<uint32>(q - p) + 64);

        char* r = (char*) mb.alloc(63, 64);
        EXPECT(((size_t)r & 63) == 0);
        EXPECT(r == q + 64);

        q = (char*) mb.alloc(31);
        EXPECT(q == r + 64);

        r = (char*) mb.alloc(63, 64);
        EXPECT(r == q + 64);

        uint32 x = god::cast<uint32>(r - p) + 64;
        uint32 k = mb.blk_size() - x;
        if ((k & 63) == 0) {
            q = (char*) mb.alloc(k, 64);
            EXPECT(q == r + 64);
            EXPECT(p + mb.blk_size() == q + k);
            EXPECT_EQ(mb.size(), 1);
        } else {
            q = (char*) mb.alloc(k, 64);
            EXPECT(p + mb.blk_size() != q + k);
            EXPECT_EQ(mb.size(), 2);
        }

        q = (char*) mb.alloc(63, 64);
        EXPECT(q < p || q >= p + mb.blk_size());
    }

    DEF_case(small) {
        void* p = co::alloc(2048);
        EXPECT_NE(p, (void*)0);
        *(uint32*)p = 7;
        EXPECT_EQ(*(uint32*)p, 7);
        co::free(p, 2048);

        void* x = p;
        p = co::alloc(8);
        EXPECT_EQ(p, x);
        co::free(p, 8);

        p = co::alloc(72);
        EXPECT_EQ(p, x);
        co::free(p, 72);

        p = co::alloc(4096);
        EXPECT_NE(p, (void*)0);
        EXPECT_NE(p, x);
        co::free(p, 4096);

        p = co::alloc(15, 32);
        EXPECT(((size_t)p & 31) == 0);

        void* a = co::alloc(31, 32);
        EXPECT_EQ((size_t)a - (size_t)p, 32);
        co::free(a, 31);

        void* b = co::alloc(31, 64);
        EXPECT(((size_t)b & 63) == 0);
        EXPECT_EQ(god::align_up((size_t)p + 32, 64), (size_t)b);

        void* c = co::alloc(223, 128);
        EXPECT(((size_t)c & 127) == 0);

        void* d = co::alloc(31, 1024);
        EXPECT(((size_t)d & 1023) == 0);

        co::free(d, 31);
        co::free(c, 223);
        co::free(b, 31);
        co::free(p, 15);

        int* v = co::make<int>(7);
        EXPECT_EQ(*v, 7);
        co::del(v);
    }

    DEF_case(realloc) {
        void* p;
        p = co::alloc(48);
        EXPECT_NE(p, (void*)0);
        *(uint32*)p = 7;
        EXPECT_EQ(*(uint32*)p, 7);

        void* x = p;
        p = co::realloc(p, 48, 64);
        EXPECT_EQ(p, x);
        EXPECT_EQ(*(uint32*)p, 7);

        p = co::try_realloc(p, 64, 128);
        EXPECT_EQ(p, x);

        p = co::realloc(p, 128, 2048);
        EXPECT_EQ(p, x);

        p = co::realloc(p, 2048, 4096);
        EXPECT_NE(p, x);
        EXPECT_EQ(*(uint32*)p, 7);

        x = p;
        p = co::realloc(p, 4096, 8 * 1024);
        EXPECT_EQ(p, x);
        EXPECT_EQ(*(uint32*)p, 7);

        p = co::realloc(p, 8 *1024, 32 * 1024);
        EXPECT_EQ(p, x);

        p = co::realloc(p, 32 * 1024, 64 * 1024);
        EXPECT_EQ(p, x);

        x = p;
        p = co::realloc(p, 64 * 1024, 132 * 1024);
        EXPECT_NE(p, x);
        EXPECT_EQ(*(uint32*)p, 7);

        p = co::realloc(p, 132 * 1024, 256 * 1024);
        EXPECT_EQ(*(uint32*)p, 7);
        co::free(p, 256 * 1024);
    }

    DEF_case(static) {
        int* x = co::make_static<int>(7);
        EXPECT_NE(x, (void*)0);
        EXPECT_EQ(*x, 7);

        int* r = co::make_rootic<int>(7);
        EXPECT_EQ(*r, 7);
    }

    static int gc = 0;
    static int gd = 0;

    struct A {
        A() {}
        virtual ~A() {}
    };

    struct B : A {
        B() { ++gc; }
        virtual ~B() { ++gd; }
    };

    DEF_case(unique) {
        co::unique<int> p;
        EXPECT(p == NULL);
        EXPECT(!p);

        p = co::make_unique<int>(7);
        EXPECT_EQ(*p, 7);
        *p = 3;
        EXPECT_EQ(*p, 3);

        auto q = co::make_unique<int>(7);
        EXPECT_EQ(*q, 7);

        q = std::move(p);
        EXPECT_EQ(*q, 3);
        EXPECT(p == NULL);

        p = q;
        EXPECT_EQ(*p, 3);
        EXPECT(q == NULL);

        p.swap(q);
        EXPECT_EQ(*q, 3);
        EXPECT(p == NULL);

        q.reset();
        EXPECT(q == NULL);

        co::unique<A> a = co::make_unique<B>();
        EXPECT_EQ(gc, 1);
        a.reset();
        EXPECT_EQ(gd, 1);
    }

    DEF_case(shared) {
        co::shared<int> p;
        EXPECT(p == NULL);
        EXPECT(!p);
        EXPECT_EQ(p.ref_count(), 0);

        co::shared<int> q(p);
        EXPECT_EQ(p.ref_count(), 0);
        EXPECT_EQ(q.ref_count(), 0);

        p = co::make_shared<int>(7);
        EXPECT_EQ(*p, 7);
        *p = 3;
        EXPECT_EQ(*p, 3);
        EXPECT_EQ(p.ref_count(), 1);
        EXPECT_EQ(q.ref_count(), 0);

        q = p;
        EXPECT_EQ(p.ref_count(), 2);
        EXPECT_EQ(q.ref_count(), 2);
        EXPECT_EQ(*q, 3);

        p.reset();
        EXPECT(p == NULL);
        EXPECT_EQ(q.ref_count(), 1);
        EXPECT_EQ(*q, 3);

        p.swap(q);
        EXPECT(q == NULL);
        EXPECT_EQ(p.ref_count(), 1);
        EXPECT_EQ(*p, 3);

        q = std::move(p);
        EXPECT(p == NULL);
        EXPECT_EQ(q.ref_count(), 1);
        EXPECT_EQ(*q, 3);

        co::shared<A> a = co::make_shared<B>();
        EXPECT_EQ(gc, 2);

        auto b = a;
        EXPECT_EQ(gc, 2);
        EXPECT_EQ(a.ref_count(), 2);

        b.reset();
        EXPECT_EQ(gd, 1);
        EXPECT_EQ(a.ref_count(), 1);

        a.reset();
        EXPECT_EQ(a.ref_count(), 0);
        EXPECT_EQ(gd, 2);
    }
}

} // namespace test
