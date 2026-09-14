#pragma once

#include "../bitops.h"
#include "co/mem.h"

namespace co {

struct IdGen {
    static constexpr size_t F = (size_t)-1;
    static constexpr size_t I = (size_t)1;
    static constexpr size_t N = sizeof(size_t) * 8;
    static constexpr int B = N == 64 ? 6 : 5;

    struct L1 {
        uint32 c1; // number of id allocated in L1
        size_t bs;
        size_t* s; // N x size_t 
    };

    struct L2 {
        uint32 c2; // number of allocated L1 (L1.s != nullptr)
        size_t bs;
        L1* l1; // N x L1
    };

    struct L3 {
        size_t bs;
        L2* l2; // N x L2
    };

    static inline void* _zalloc(size_t n) {
        void* p = co::alloc(n, co::cache_line_size);
        runtime_assert(p);
        ::memset(p, 0, n);
        return p;
    }

    IdGen() {
        _l3.bs = 0;
        _l3.l2 = (L2*) _zalloc(N * sizeof(L2));
    }

    ~IdGen() {
        for (int i = 0; i < N; ++i) {
            L2& l2 = _l3.l2[i];
            if (l2.l1) {
                for (int k = 0; k < N; ++k) {
                    L1& l1 = l2.l1[k];
                    if (l1.s) co::free(l1.s, N * sizeof(size_t));
                }
                co::free(l2.l1, N * sizeof(L1));
            }
        }
        co::free(_l3.l2, N * sizeof(L2));
    }

    static inline void _set(size_t& x, int i) {
        x |= (I << i);
    }

    static inline void _unset(size_t& x, int i) {
        x &= ~(I << i);
    }

    static inline size_t _fetch_and_unset(size_t& x, int i) {
        const size_t o = x;
        x &= ~(I << i);
        return o;
    }

    static inline size_t _set_and_fetch(size_t& x, int i) {
        return x |= (I << i);
    }

    int pop() {
        L1* l1;
        L2* l2;
        int b0, b1, b2, b3;

        b3 = find_lsb(~_l3.bs);
        runtime_assert(b3 >= 0, "Too many coroutines...");

        l2 = _l3.l2 + b3;
        if (l2->l1) goto _find_in_l2;
        l2->l1 = (L1*) _zalloc(N * sizeof(L1));

    _find_in_l2:
        b2 = find_lsb(~l2->bs);
        l1 = l2->l1 + b2;
        if (l1->s) goto _find_in_l1;
        ++l2->c2;
        l1->s = (size_t*) _zalloc(N * sizeof(size_t));

    _find_in_l1:
        b1 = find_lsb(~l1->bs);
        b0 = find_lsb(~l1->s[b1]);
        if (_set_and_fetch(l1->s[b1], b0) != F) goto _end;
        if (_set_and_fetch(l1->bs, b1) != F) goto _end;
        if (_set_and_fetch(l2->bs, b2) != F) goto _end;
        _set(_l3.bs, b3);

    _end:
        ++l1->c1;
        return (b3 << (B * 3)) + (b2 << (B * 2)) + (b1 << B) + b0;
    }

    void push(int id) {
        const int b3 = id >> (B * 3);
        const int r3 = id & ((1 << (B * 3)) - 1);
        const int b2 = r3 >> (B * 2);
        const int r2 = r3 & ((1 << (B * 2)) - 1);
        const int b1 = r2 >> B;
        const int b0 = r2 & ((1 << B) - 1);

        L2& l2 = _l3.l2[b3];
        L1& l1 = l2.l1[b2];
        if (_fetch_and_unset(l1.s[b1], b0) != F) goto _xx;
        if (_fetch_and_unset(l1.bs, b1) != F) goto _xx;
        if (_fetch_and_unset(l2.bs, b2) != F) goto _xx;
        _unset(_l3.bs, b3);

    _xx:
        if (--l1.c1 > 0) goto _end;
        co::free(l1.s, N * sizeof(size_t));
        l1.s = nullptr;

        if (--l2.c2 > 0) goto _end;
        co::free(l2.l1, N * sizeof(L1));
        l2.l1 = nullptr;

    _end:
        return;
    }

    L3 _l3;
};

} // co
