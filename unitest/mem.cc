#include "co/unitest.h"
#include "co/align.h"
#include "co/clist.h"
#include "co/mem.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef _MSC_VER
#pragma warning (disable:4200)
#endif
#include <windows.h>
#include <intrin.h>
#endif

#ifdef _WIN32
inline int _find_msb(size_t x) {
    unsigned long r;
    _BitScanReverse64(&r, x);
    return (int)r;
}

inline uint32 _find_lsb(size_t x) {
    unsigned long r;
    _BitScanForward64(&r, x); // x != 0
    return r;
}

#else
inline int _find_msb(size_t x) {
    return 63 - __builtin_clzll(x); // x != 0
}

inline uint32 _find_lsb(size_t x) {
    return __builtin_ffsll(x) - 1;
}
#endif


namespace test {

struct Destruct {
    using _D = co::_D;
    struct _Memb : co::clink {
        _D p[];
    };

    static_assert(alignof(_D) == sizeof(void*), "");
    static const size_t BLK_SIZE = 8192;
    static const size_t MAX_POS = (BLK_SIZE - sizeof(_Memb)) / (sizeof(_D));

    Destruct() : _h(0), _pos(0) {}
    ~Destruct() {
        _Memb* const h = _h;
        for (_Memb* b = h; b; b = (_Memb*)b->next) {
            _D* const d = b->p;
            const size_t n = (b != h ? MAX_POS : _pos);
            for (size_t x = n; x > 0; --x) d[x - 1]();
        }
    }

    void add_destructor(_D&& d) {
        if (!_l.empty() && _pos < MAX_POS) goto _end;
        _l.push_front((_Memb*)::malloc(BLK_SIZE));
        _pos = 0;
    _end:
        new(_h->p + _pos++) _D(std::forward<_D>(d));
    }

    union {
        _Memb* _h;
        co::clist _l;
    };
    size_t _pos;
};

struct StaticAlloc {
    struct _Memb : co::clink {
        size_t blk_size;
        char p[];
    };
    static_assert(alignof(_Memb) == sizeof(void*), "");

    StaticAlloc() : _h(0), _pos(0) {}

    ~StaticAlloc() {
        _l.for_each([](co::clink* c) { ::free(c); });
        _l.clear();
    }

    void* alloc(size_t n, size_t align);

    union {
        _Memb* _h;
        co::clist _l;
    };
    size_t _pos;
};

void* StaticAlloc::alloc(size_t n, size_t align) {
    if (align < sizeof(void*)) align = sizeof(void*);
    n = co::align_up(n, align);

    if (_l.empty()) goto new_block;
    {
        char* p = _h->p + _pos;
        if (align != sizeof(void*)) p = co::align_up(p, align);
        if ((char*)_h + _h->blk_size < p + n) goto new_block;
        _pos = (size_t)(p - _h->p + n);
        return p;
    }

new_block:
    if (n <= 8192) {
        const size_t blk_size = n <= 4096 ? 8192 : 16 * 1024;
        _Memb* m = (_Memb*) ::malloc(blk_size);
        runtime_assert(m);
        _l.push_front(m);
        m->blk_size = blk_size;
        char* p = align != sizeof(void*) ? co::align_up(m->p, align) : m->p;
        _pos = (size_t)(p - _h->p + n);
        return p;
    }

    {
        const size_t blk_size = n + align + sizeof(_Memb);
        _Memb* m = (_Memb*) ::malloc(blk_size);
        runtime_assert(m);
        _l.push_back(m);
        m->blk_size = blk_size;
        _pos = n + align;
        return align != sizeof(void*) ? co::align_up(m->p, align) : m->p;
    }
}

static int g_mem_v = 0;

struct M {
    M() = default;
    ~M() { ++g_mem_v; }
};

struct K {
    K() = default;
    ~K() { g_mem_v = 123; }
};

DEF_test(mem) {
    DEF_case(bitops) {
        EXPECT_EQ(_find_lsb(1), 0);
        EXPECT_EQ(_find_lsb(12), 2);
        EXPECT_EQ(_find_lsb(3u << 20), 20);
        EXPECT_EQ(_find_msb(1), 0);
        EXPECT_EQ(_find_msb(12), 3);
        EXPECT_EQ(_find_msb(3u << 20), 21);
        EXPECT_EQ(_find_msb(1ull << 63), 63);
        EXPECT_EQ(_find_msb(~0ull), 63);
        EXPECT_EQ(_find_lsb(~0ull), 0);
    }

    DEF_case(StaticAlloc) {
        StaticAlloc m;
        char* p = (char*) m.alloc(15, 8);
        EXPECT(p == m._h->p);
        EXPECT_EQ(m._pos, 16);

        char* q = (char*) m.alloc(63, 64);
        EXPECT_EQ((size_t)q & 63, 0);
        EXPECT_EQ(m._pos, static_cast<uint32>(q - p) + 64);

        char* r = (char*) m.alloc(63, 64);
        EXPECT(((size_t)r & 63) == 0);
        EXPECT(r == q + 64);

        q = (char*) m.alloc(31, 8);
        EXPECT(q == r + 64);

        r = (char*) m.alloc(63, 64);
        EXPECT(r == q + 64);

        uint32 x = static_cast<uint32>(r - p) + 64;
        EXPECT_EQ(x, m._pos);

        uint32 k = (uint32)(m._h->blk_size - x - sizeof(StaticAlloc::_Memb));
        q = (char*) m.alloc(k, 8);
        EXPECT(q == r + 64);
        EXPECT((char*)m._h + m._h->blk_size == q + k);
        EXPECT(m._h && !m._h->next);

        q = (char*) m.alloc(63, 64);
        EXPECT(m._h && m._h->next && !m._h->next->next);
        EXPECT(q < p || q >= (p + m._h->blk_size - sizeof(StaticAlloc::_Memb)));
    }

    DEF_case(Destruct) {
        M m;
        K k;
        {
            Destruct da;
            da.add_destructor(co::_D(&m));
            da.add_destructor(co::_D(&m));
            EXPECT(da._h && !da._h->next);

            for (int i = 0; i < 8192 / sizeof(co::_D); ++i) {
                da.add_destructor(co::_D(&m));
            }
            EXPECT(da._h && da._h->next && !da._h->next->next);

            da.add_destructor(co::_D(&k));
        }
        EXPECT_NE(g_mem_v, 123);
        EXPECT_EQ(g_mem_v, 8192 / sizeof(co::_D) + 2 + 123);
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
        EXPECT_EQ(co::align_up((size_t)p + 32, 64), (size_t)b);

        void* c = co::alloc(223, 128);
        EXPECT(((size_t)c & 127) == 0);

        void* d = co::alloc(31, 256);
        EXPECT(((size_t)d & 255) == 0);

        co::free(d, 31);
        co::free(c, 223);
        co::free(b, 31);
        co::free(p, 15);

        int* v = co::_new<int>(7);
        EXPECT_EQ(*v, 7);
        co::_delete(v);
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

        p = co::realloc(p, 64, 2048);
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
