#ifdef __linux__
#define _GNU_SOURCE  // mremap
#endif

#include "co/mem.h"
#include "co/align.h"
#include "co/clist.h"
#include "bitops.h"
#include <stdlib.h>
#include <cstddef>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef _MSC_VER
#pragma warning (disable:4200)
#endif
#include <windows.h>
#include <memoryapi.h>
#else
#include <sys/mman.h>
#endif

#ifdef _WIN32
inline void* _vm_alloc(size_t n) {
    return VirtualAlloc(NULL, n, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

inline void* _vm_reserve(size_t n) {
    return VirtualAlloc(NULL, n, MEM_RESERVE, PAGE_READWRITE);
}

inline bool _vm_commit(void* p, size_t n) {
    return VirtualAlloc(p, n, MEM_COMMIT, PAGE_READWRITE) == p;
}

inline void _vm_decommit(void* p, size_t n) {
    VirtualFree(p, n, MEM_DECOMMIT);
}

inline void _vm_free(void* p, size_t n) {
    VirtualFree(p, 0, MEM_RELEASE);
}

#else
inline void* _vm_alloc(size_t n) {
    void* const p = ::mmap(
        NULL, n, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
    );
    return p != MAP_FAILED ? p : NULL;
}

// freebsd has no MAP_NORESERVE, define it as 0
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

inline void* _vm_reserve(size_t n) {
    void* const p = ::mmap(
        NULL, n, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0
    );
    return p != MAP_FAILED ? p : NULL;
}

inline bool _vm_commit(void* p, size_t n) {
    return ::mmap(
        p, n, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0
    ) == p;
}

inline void _vm_decommit(void* p, size_t n) {
    (void) ::mmap(
        p, n, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0
    );
}

inline void _vm_free(void* p, size_t n) {
    ::munmap(p, n);
}
#endif

#ifdef __linux__
inline void* _vm_realloc(void* p, size_t o, size_t n) {
    void* const x = ::mremap(p, o, n, MREMAP_MAYMOVE);
    return x != MAP_FAILED ? x : nullptr;
}
#else
inline void* _vm_realloc(void*, size_t, size_t) {
    return nullptr;
}
#endif

namespace co {

struct Destruct {
    struct _Memb : co::clink {
        _D p[];
    };

    static_assert(alignof(_D) == sizeof(void*), "");
    static const size_t BLK_SIZE = 8192;
    static const size_t MAX_POS = (BLK_SIZE - sizeof(_Memb)) / sizeof(_D);

    Destruct() : _h(0), _pos(0) {}
    ~Destruct() {
        const auto h = (_Memb*)_l.front();
        for (co::clink* c = h; c;) {
            const auto b = (_Memb*)c;
            c = c->next;
            _D* const d = b->p;
            const size_t n = (b != h ? MAX_POS : _pos);
            for (size_t x = n; x > 0; --x) d[x - 1]();
            ::free(b);
        }
    }

    void add_destructor(_D&& d) {
        if (_l.empty() || _pos >= MAX_POS) {
            _Memb* m = (_Memb*) ::malloc(BLK_SIZE);
            runtime_assert(m);
            _l.push_front(m);
            _pos = 0;
        }
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

struct Root {
    Root() : _mtx(), _sa() {}
    ~Root() = default;

    template<typename T, typename... Args>
    T* make(Args&&... args) {
        void* p;
        {
            std::lock_guard<std::mutex> g(_mtx);
            p = _sa.alloc(sizeof(T), alignof(T));
            _da.add_destructor(_D((T*)p));
        }
        return new(p) T(std::forward<Args>(args)...);
    }

    void add_destructor(_D&& d, int i) {
        std::lock_guard<std::mutex> g(_mtx);
        _dx[i].add_destructor(std::forward<_D>(d));
    }

    std::mutex _mtx;
    StaticAlloc _sa; // alloc memory for GlobalAlloc and ThreadAlloc
    Destruct _da;    // used to destruct GlobalAlloc and ThreadAlloc
    Destruct _dx[4]; // 0: _rootic, 1: _static, 2: rootic, 3: static 
};

#if __arch64
constexpr uint32 B = 6;
constexpr uint32 g_array_size = 32;
#else
constexpr uint32 B = 5;
constexpr uint32 g_array_size = 4;
#endif
constexpr uint32 R = (1 << B) - 1;
constexpr size_t C = (size_t)1;
constexpr uint32 g_su_bits = 4;               // bits of small alloc units
constexpr uint32 g_lu_bits = 12;              // bits of large alloc units
constexpr uint32 g_su_size = 1 << g_su_bits;  // small alloc units (16)
constexpr uint32 g_lu_size = 1 << g_lu_bits;  // large alloc units (4k)
constexpr uint32 g_sb_bits = 16;               // bits of small block
constexpr uint32 g_lb_bits = g_sb_bits + B;    // bits of large block
constexpr uint32 g_hb_bits = g_lb_bits + B;    // bits of huge block
constexpr uint32 g_sb_size = 1 << g_sb_bits;   // size of small block
constexpr uint32 g_lb_size = 1 << g_lb_bits;   // size of large block
constexpr uint32 g_hb_size = 1 << g_hb_bits;   // size of huge block
constexpr size_t g_max_small_size = 3584;      // 3.5k
constexpr size_t g_max_medium_size = 1u << 17; // 128k
using xx::g_max_align;

template<typename T, typename V>
inline T _fetch_add(T* p, V v) {
    const T x = *p;
    *p += v;
    return x;
}

template<typename T, typename V>
inline T _fetch_and(T* p, V v) {
    const T x = *p;
    *p &= (T)v;
    return x;
}

struct Bitset {
    explicit Bitset(void* s) : _s((size_t*)s) {}

    void set(uint32 i) {
        _s[i >> B] |= (C << (i & R));
    }

    void unset(uint32 i) {
        _s[i >> B] &= ~(C << (i & R));
    }

    bool test_and_unset(uint32 i) {
        const size_t x = (C << (i & R));
        return _fetch_and(&_s[i >> B], ~x) & x;
    }

    int rfind(uint32 i) const {
        int n = static_cast<int>(i >> B);
        do {
            const size_t x = _s[n];
            if (x) return co::find_msb(x) + (n << B);
        } while (--n >= 0);
        return -1;
    }

    void atomic_set(uint32 i) {
        co::atomic_or(&_s[i >> B], C << (i & R), mo_relaxed);
    }

    size_t* _s;
};

// manage and alloc large blocks
struct HugeBlock : co::clink {
    explicit HugeBlock(void* p) : _p((char*)p) {}

    void* alloc() {
        const uint32 i = co::find_lsb(~_bits);
        if (i < R) {
            _bits |= (C << i);
            return _p + (((size_t)i) << g_lb_bits);
        }
        return NULL;
    }

    bool free(void* p) {
        const uint32 i = (uint32)(((char*)p - _p) >> g_lb_bits);
        return (_bits &= ~(C << i)) == 0;
    }

    char* _p; // beginning address to alloc
    size_t _bits;
};

inline HugeBlock* make_huge_block() {
    void* x = _vm_reserve(g_hb_size);
    if (x) {
        if (_vm_commit(x, 4096)) {
            void* p = co::align_up<g_lb_size>(x);
            if (p == x) p = (char*)x + g_lb_size;
            return new (x) HugeBlock(p);
        }
        _vm_free(x, g_hb_size);
    }
    return NULL;
}

// manage and alloc small blocks
struct LargeBlock : co::clink {
    explicit LargeBlock(HugeBlock* parent)
        : _p((char*)this + g_sb_size), _parent(parent) {
    }

    void* alloc() {
        const uint32 i = co::find_lsb(~_bits);
        if (i < R) {
            _bits |= (C << i);
            return _p + (((size_t)i) << g_sb_bits);
        }
        return NULL;
    }

    bool free(void* p) {
        const uint32 i = (uint32)(((char*)p - _p) >> g_sb_bits);
        return (_bits &= ~(C << i)) == 0;
    }

    HugeBlock* parent() const { return _parent; }

    char* _p; // beginning address to alloc
    size_t _bits;
    HugeBlock* _parent;
};

// thread-local allocator
struct ThreadAlloc;

// alloc memory from 4K to 128K
// | LargeAlloc | _bs | padding | _xbs |
struct LargeAlloc : co::clink {
    static const uint32 BS_BITS = g_lb_size / g_lu_size;
    static const uint32 BS_SIZE = BS_BITS >> 3;
    static const uint32 LA_SIZE = 1 << B;
    static const uint32 MAX_BIT = BS_BITS - 1;

    explicit LargeAlloc(HugeBlock* parent, ThreadAlloc* ta)
        : _parent(parent), _ta(ta) {
        static_assert(sizeof(*this) == LA_SIZE);
        _p = (char*)this + g_lu_size;
        _pbs = (char*)this + LA_SIZE;
        _xpbs = (char*)this + co::align_up<co::cache_line_size>(LA_SIZE + BS_SIZE);
    }

    // alloc n units
    void* alloc(uint32 n) {
        if (_bit + n <= MAX_BIT) {
            _bs.set(_bit);
            return _p + (_fetch_add(&_bit, n) << g_lu_bits);
        }
        return NULL;
    }

    void* try_hard_alloc(uint32 n);

    bool free(void* p) {
        int i = (int)(((char*)p - _p) >> g_lu_bits);
        //runtime_assert(_bs.test_and_unset(i));
        _bs.unset(i);
        const int r = _bs.rfind(_bit);
        return r < i ? ((_bit = r >= 0 ? i : 0) == 0) : false;
    }

    void xfree(void* p) {
        const uint32 i = (uint32)(((char*)p - _p) >> g_lu_bits);
        _xbs.atomic_set(i);
    }

    void* realloc(void* p, uint32 o, uint32 n) {
        uint32 i = (uint32)(((char*)p - _p) >> g_lu_bits);
        if (_bit == i + o && i + n <= MAX_BIT) {
            _bit = i + n;
            return p;
        }
        return NULL;
    }

    HugeBlock* parent() const { return _parent; }
    ThreadAlloc* talloc() const { return _ta; }

    char* _p;    // beginning address to alloc
    uint32 _bit; // current bit
    union {
        Bitset _bs;
        char* _pbs;
    };
    union {
        Bitset _xbs;
        char* _xpbs;
    };
    HugeBlock* _parent;
    ThreadAlloc* _ta;
};

void* LargeAlloc::try_hard_alloc(uint32 n) {
    size_t* const p = (size_t*)_pbs;
    size_t* const q = (size_t*)_xpbs;

    int i = _bit >> B;
    while (p[i] == 0) --i;
    for (size_t x = co::atomic_load(&q[i], mo_relaxed); x != 0;) {
        co::atomic_and(&q[i], ~x, mo_relaxed);
        p[i] &= ~x;
        const int lsb = static_cast<int>(co::find_lsb(x) + (i << B));
        const int r = _bs.rfind(_bit);
        if (r >= lsb) break;
        _bit = r >= 0 ? lsb : 0;
        if (_bit == 0) break;
        if (--i < 0) break;
        x = co::atomic_load(&q[i], mo_relaxed);
    }

    if (_bit + n <= MAX_BIT) {
        _bs.set(_bit);
        return _p + (_fetch_add(&_bit, n) << g_lu_bits);
    }
    return NULL;
}

// alloc memory from 16 to 2K
// | SmallAlloc | _bs | padding | _xbs |
struct SmallAlloc : co::clink {
    static const uint32 BS_BITS = g_sb_size / g_su_size;
    static const uint32 BS_SIZE = BS_BITS >> 3;
    static const uint32 SA_SIZE = 1 << B;
    static const uint32 SB_SIZE = co::align_up<co::cache_line_size>(SA_SIZE + BS_SIZE);
    static const uint32 SUM_SIZE = SB_SIZE + BS_SIZE;
    static const uint32 MAX_BIT = BS_BITS - (SUM_SIZE >> g_su_bits);
    static_assert(BS_SIZE >= co::cache_line_size && !(BS_SIZE & (BS_SIZE - 1)), "");
    static_assert(alignof(std::max_align_t) <= 16, "");

    explicit SmallAlloc(LargeBlock* parent, ThreadAlloc* ta)
        : _bit(0), _parent(parent), _ta(ta) {
        static_assert(sizeof(*this) == SA_SIZE, "");
        next = prev = 0;
        _p = (char*)this + SUM_SIZE;
        _pbs = (char*)this + SA_SIZE;
        _xpbs = (char*)this + SB_SIZE;
    }

    // alloc n units
    void* alloc(uint32 n) {
        if (_bit + n <= MAX_BIT) {
            _bs.set(_bit);
            return _p + (_fetch_add(&_bit, n) << g_su_bits);
        }
        return NULL;
    }

    void* alloc(uint32 n, uint32 a) {
        void* p = NULL;
        const uint32 bit = (a <= (co::cache_line_size >> g_su_bits))
            ? co::align_up(_bit, a)
            : co::align_up(_bit, a) + ((uint32)(co::align_up(_p, a << g_su_bits) - _p) >> g_su_bits);

        n = co::align_up(n, a);
        if (bit + n <= MAX_BIT) {
            _bs.set(bit);
            p = _p + (bit << g_su_bits);
            _bit = bit + n;
        }
        return p;
    }

    void* try_hard_alloc(uint32 n);

    bool free(void* p) {
        const int i = (int)(((char*)p - _p) >> g_su_bits);
        //runtime_assert(_bs.test_and_unset(i));
        _bs.unset(i);
        const int r = _bs.rfind(_bit);
        return r < i ? ((_bit = r >= 0 ? i : 0) == 0) : false;
    }

    void xfree(void* p) {
        const uint32 i = (uint32)(((char*)p - _p) >> g_su_bits);
        _xbs.atomic_set(i);
    }

    void* realloc(void* p, uint32 o, uint32 n) {
        uint32 i = (uint32)(((char*)p - _p) >> g_su_bits);
        if (_bit == i + o && i + n <= MAX_BIT) {
            _bit = i + n;
            return p;
        }
        return NULL;
    }

    LargeBlock* parent() const { return _parent; }
    ThreadAlloc* talloc() const { return _ta; }

    char* _p; // beginning address to alloc
    uint32 _bit;
    union {
        Bitset _bs;
        char* _pbs;
    };
    union {
        Bitset _xbs;
        char* _xpbs;
    };
    LargeBlock* _parent;
    ThreadAlloc* _ta;
};

void* SmallAlloc::try_hard_alloc(uint32 n) {
    size_t* const p = (size_t*)_pbs;
    size_t* const q = (size_t*)_xpbs;

    int i = _bit >> B;
    while (p[i] == 0) --i;
    size_t x = co::atomic_load(&q[i], mo_relaxed);
    if (x) {
        for (;;) {
            if (x) {
                co::atomic_and(&q[i], ~x, mo_relaxed);
                p[i] &= ~x;
                const int lsb = static_cast<int>(co::find_lsb(x) + (i << B));
                const int r = _bs.rfind(_bit);
                if (r >= lsb) break;
                _bit = r >= 0 ? lsb : 0;
                if (_bit == 0) break;
            }
            if (--i < 0) break;
            x = co::atomic_load(&q[i], mo_relaxed);
        }
    }

    if (_bit + n <= MAX_BIT) {
        _bs.set(_bit);
        return _p + (_fetch_add(&_bit, n) << g_su_bits);
    }
    return NULL;
}

// manage huge blocks, and alloc large blocks
//   - shared by all threads
struct GlobalAlloc {
    GlobalAlloc() = default;
    ~GlobalAlloc() {
        for (uint32 i = 0; i < g_array_size; ++i) {
            std::lock_guard<std::mutex> g(_x[i].mtx);
            HugeBlock *h = _x[i].hb, *next;
            while (h) {
                next = (HugeBlock*) h->next;
                _vm_free(h, g_hb_size);
                h = next;
            }
        }
    }

    struct __cacheline_aligned X {
        X() : mtx(), hb(0) {}
        std::mutex mtx;
        union {
            HugeBlock* hb;
            co::clist lhb;
        };
    };

    static_assert(sizeof(X) <= 256, "");
    static_assert(g_array_size <= 32, "");

    void* alloc(uint32 alloc_id, HugeBlock** parent);
    LargeBlock* make_large_block(uint32 alloc_id);
    LargeAlloc* make_large_alloc(uint32 alloc_id);
    void free(void* p, HugeBlock* hb, uint32 alloc_id);

    X _x[g_array_size];
};

struct __cacheline_aligned ThreadAlloc {
    ThreadAlloc(GlobalAlloc* ga);
    ~ThreadAlloc() = default;

    uint32 id() const { return _id; }
    void* alloc(size_t n);
    void* alloc(size_t n, size_t align);
    void free(void* p, size_t n);
    void* realloc(void* p, size_t o, size_t n);
    void* salloc(size_t n, size_t a) { return _s.alloc(n, a); }

    union { LargeBlock* _lb; co::clist _llb; };
    union { LargeAlloc* _la; co::clist _lla; };
    union { SmallAlloc* _sa; co::clist _lsa; };
    uint32 _id;
    GlobalAlloc* _ga;
    StaticAlloc _s;
};

struct __cacheline_aligned {
    char _[sizeof(Root)];
    uint32 alloc_id;
} g_buf;

static Root* g_root;
static GlobalAlloc* g_ga;
__thread ThreadAlloc* g_ta;

namespace xx {

static int g_nifty_counter;

MemInit::MemInit() {
    if (g_nifty_counter++ == 0) {
        g_root = new (&g_buf) Root();
        g_buf.alloc_id = (uint32)-1;
        g_ga = g_root->make<GlobalAlloc>();
    }
}

MemInit::~MemInit() {
    if (--g_nifty_counter == 0) g_root->~Root();
}

} // xx

inline ThreadAlloc::ThreadAlloc(GlobalAlloc* ga)
    : _lb(0), _la(0), _sa(0), _ga(ga), _s() {
    _id = co::atomic_inc(&g_buf.alloc_id, mo_relaxed);
}

inline ThreadAlloc* talloc() {
    const auto ta = g_ta;
    return ta ? ta : (g_ta = g_root->make<ThreadAlloc>(g_ga));
}

#define _try_alloc(l, n, k) \
    const auto h = l.front(); \
    auto k = h->next; \
    l.move_back(h); \
    for (int i = 0; i < n && k != h; k = k->next, ++i)

inline void* GlobalAlloc::alloc(uint32 alloc_id, HugeBlock** parent) {
    void* p = NULL;
    auto& x = _x[alloc_id & (g_array_size - 1)];

    do {
        std::lock_guard<std::mutex> g(x.mtx);
        if (x.hb && (p = x.hb->alloc())) {
            *parent = x.hb;
            goto end;
        }
        if (x.hb && x.hb->next) {
            _try_alloc(x.lhb, 8, k) {
                if ((p = ((HugeBlock*)k)->alloc())) {
                    *parent = (HugeBlock*)k;
                    x.lhb.move_front(k);
                    goto end;
                }
            }
        }
        {
            auto hb = make_huge_block();
            if (hb) {
                x.lhb.push_front(hb);
                p = hb->alloc();
                *parent = hb;
            }
        }
    } while (0);

end:
    if (p) {
        if (_vm_commit(p, g_lb_size)) return p;
        (*parent)->free(p);
    }
    return NULL;
}

inline void GlobalAlloc::free(void* p, HugeBlock* hb, uint32 alloc_id) {
    _vm_decommit(p, g_lb_size);
    auto& x = _x[alloc_id & (g_array_size - 1)];
    bool r;
    {
        std::lock_guard<std::mutex> g(x.mtx);
        r = hb->free(p) && hb != x.hb;
        if (r) x.lhb.erase(hb);
    }
    if (r) _vm_free(hb, g_hb_size);
}

inline LargeBlock* GlobalAlloc::make_large_block(uint32 alloc_id) {
    HugeBlock* parent;
    auto p = this->alloc(alloc_id, &parent);
    return p ? new (p) LargeBlock(parent) : NULL;
}

inline LargeAlloc* GlobalAlloc::make_large_alloc(uint32 alloc_id) {
    HugeBlock* parent;
    auto p = this->alloc(alloc_id, &parent);
    return p ? new (p) LargeAlloc(parent, talloc()) : NULL;
}

inline SmallAlloc* make_small_alloc(LargeBlock* lb, ThreadAlloc* ta) {
    auto p = lb->alloc();
    return p ? new(p) SmallAlloc(lb, ta) : NULL;
}

template<uint32 N, uint32 Bits>
constexpr uint32 nblk(uint32 x) noexcept {
    static_assert(N == (1u << Bits));
    return (x >> Bits) + !!(x & (N - 1));
}

inline void* ThreadAlloc::alloc(size_t n) {
    void* p = 0;
    SmallAlloc* sa;
    if (n <= g_max_small_size) {
        const uint32 u = n > g_su_size ? nblk<g_su_size, g_su_bits>((uint32)n) : 1;
        if (_sa && (p = _sa->alloc(u))) goto end;

        if (_sa && _sa->next) {
            _try_alloc(_lsa, 4, k) {
                if ((p = ((SmallAlloc*)k)->try_hard_alloc(u))) {
                    _lsa.move_front(k);
                    goto end;
                }
            }
        }

        if (_lb && (sa = make_small_alloc(_lb, this))) {
            _lsa.push_front(sa);
            p = sa->alloc(u);
            goto end;
        }

        if (_lb && _lb->next) {
            _try_alloc(_llb, 4, k) {
                if ((sa = make_small_alloc((LargeBlock*)k, this))) {
                    _llb.move_front(k);
                    _lsa.push_front(sa);
                    p = sa->alloc(u);
                    goto end;
                }
            }
        }

        {
            auto lb = _ga->make_large_block(_id);
            if (lb) {
                _llb.push_front(lb);
                sa = make_small_alloc(lb, this);
                _lsa.push_front(sa);
                p = sa->alloc(u);
            }
            goto end;
        }

    } else if (n <= g_max_medium_size) {
        const uint32 u = nblk<g_lu_size, g_lu_bits>((uint32)n);
        if (_la && (p = _la->alloc(u))) goto end;

        if (_la && _la->next) {
            _try_alloc(_lla, 4, k) {
                if ((p = ((LargeAlloc*)k)->try_hard_alloc(u))) {
                    _lla.move_front(k);
                    goto end;
                }
            }
        }

        {
            auto la = _ga->make_large_alloc(_id);
            if (la) {
                _lla.push_front(la);
                p = la->alloc(u);
            }
            goto end;
        }

    } else {
        p = _vm_alloc(n);
    }

end:
    return p;
}

inline void* ThreadAlloc::alloc(size_t n, size_t align) {
    if (align <= (g_su_size)) return this->alloc(n);
    runtime_assert(align <= g_max_align && !(align & (align - 1)));

    void* p = 0;
    SmallAlloc* sa;
    if (n <= g_max_small_size) {
        const uint32 a = (uint32)align >> g_su_bits;
        const uint32 u = n > g_su_size ? nblk<g_su_size, g_su_bits>((uint32)n) : 1;
        if (_sa && (p = _sa->alloc(u, a))) goto end;

        if (_lb && (sa = make_small_alloc(_lb, this))) {
            _lsa.push_front(sa);
            p = sa->alloc(u, a);
            goto end;
        }

        if (_lb && _lb->next) {
            _try_alloc(_llb, 4, k) {
                if ((sa = make_small_alloc((LargeBlock*)k, this))) {
                    _llb.move_front(k);
                    _lsa.push_front(sa);
                    p = sa->alloc(u, a);
                    goto end;
                }
            }
        }

        {
            auto lb = _ga->make_large_block(_id);
            if (lb) {
                _llb.push_front(lb);
                sa = make_small_alloc(lb, this);
                _lsa.push_front(sa);
                p = sa->alloc(u, a);
            }
            goto end;
        }

    } else {
        static_assert(g_max_align <= g_lu_size, "");
        p = this->alloc(n);
    }

end:
    return p;
}

inline void ThreadAlloc::free(void* p, size_t n) {
    if (p) {
        if (n <= g_max_small_size) {
            const auto sa = (SmallAlloc*) co::align_down<g_sb_size>(p);
            const auto ta = sa->talloc();
            if (ta == this) {
                if (sa->free(p) && sa != _sa) {
                    _lsa.erase(sa);
                    const auto lb = sa->parent();
                    if (lb->free(sa) && lb != _lb) {
                        _llb.erase(lb);
                        _ga->free(lb, lb->parent(), _id);
                    }
                }
            } else {
                sa->xfree(p);
            }

        } else if (n <= g_max_medium_size) {
            const auto la = (LargeAlloc*) co::align_down<g_lb_size>(p);
            const auto ta = la->talloc();
            if (ta == this) {
                if (la->free(p) && la != _la) {
                    _lla.erase(la);
                    _ga->free(la, la->parent(), _id);
                }
            } else {
                la->xfree(p);
            }

        } else {
            _vm_free(p, n);
        }
    }
}

inline void* ThreadAlloc::realloc(void* p, size_t o, size_t n) {
    if (!p) return this->alloc(n);
    runtime_assert(o < n, "old size must be less than new size in realloc");

    if (o <= g_max_small_size) {
        const uint32 k = (o > g_su_size ? co::align_up<g_su_size>((uint32)o) : g_su_size);
        if (n <= (size_t)k) return p;

        const auto sa = (SmallAlloc*) co::align_down<g_sb_size>(p);
        if (sa == _sa && n <= g_max_small_size) {
            const uint32 l = nblk<g_su_size, g_su_bits>((uint32)n);
            auto x = sa->realloc(p, k >> g_su_bits, l);
            if (x) return x;
        }

    } else if (o <= g_max_medium_size) {
        const uint32 k = co::align_up<g_lu_size>((uint32)o);
        if (n <= (size_t)k) return p;

        const auto la = (LargeAlloc*) co::align_down<g_lb_size>(p);
        if (la == _la && n <= g_max_medium_size) {
            const uint32 l = nblk<g_lu_size, g_lu_bits>((uint32)n);
            auto x = la->realloc(p, k >> g_lu_bits, l);
            if (x) return x;
        }

    } else {
        auto x = _vm_realloc(p, o, n);
        if (x) return x;
    }

    auto x = this->alloc(n);
    if (x) { ::memcpy(x, p, o); this->free(p, o); }
    return x;
}

void* _static_alloc(size_t n, size_t align) {
    runtime_assert(align <= g_max_align && !(align & (align - 1)));
    return talloc()->salloc(n, align);
}

void _add_destructor(_D&& d, int x) {
    g_root->add_destructor(std::forward<_D>(d), x);
}

void* alloc(size_t n) {
    return talloc()->alloc(n);
}

void* alloc(size_t n, size_t align) {
    return talloc()->alloc(n, align);
}

void free(void* p, size_t n) {
    return talloc()->free(p, n);
}

void* realloc(void* p, size_t o, size_t n) {
    return talloc()->realloc(p, o, n);
}

void* zalloc(size_t size) {
    if (size <= g_max_medium_size) {
        auto p = co::alloc(size);
        if (p) ::memset(p, 0, size);
        return p;
    }
    return _vm_alloc(size);
}

void* valloc(size_t n) { return _vm_alloc(n); }

void vfree(void* p, size_t n) { _vm_free(p, n); }

char* strdup(const char* s) {
    const size_t n = strlen(s) + 1;
    char* const p = (char*) co::alloc(n);
    runtime_assert(p);
    ::memcpy(p, s, n);
    return p;
}

} // co
