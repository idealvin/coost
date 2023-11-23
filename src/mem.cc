#include "co/mem.h"
#include "co/atomic.h"
#include "co/clist.h"
#include "co/god.h"
#include "co/log.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <memoryapi.h>
#include <intrin.h>
#else
#include <sys/mman.h>
#endif


#ifdef _WIN32
inline void* _vm_reserve(size_t n) {
    return VirtualAlloc(NULL, n, MEM_RESERVE, PAGE_READWRITE);
}

inline void _vm_commit(void* p, size_t n) {
    void* x = VirtualAlloc(p, n, MEM_COMMIT, PAGE_READWRITE);
    assert(x == p); (void)x;
}

inline void _vm_decommit(void* p, size_t n) {
    VirtualFree(p, n, MEM_DECOMMIT);
}

inline void _vm_free(void* p, size_t n) {
    VirtualFree(p, 0, MEM_RELEASE);
}

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
#include <sys/mman.h>

inline void* _vm_reserve(size_t n) {
    void* const p = ::mmap(
        NULL, n, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0
    );
    return p != MAP_FAILED ? p : NULL;
}

inline void _vm_commit(void* p, size_t n) {
    void* x = ::mmap(
        p, n, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0
    );
    assert(x == p); (void)x;
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


namespace co {
namespace xx {

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
    char* operator[](uint32 i) const { return _m[i]; }

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
        _m[_u[-2]++] = x;
        _p = 0;
        p = align != sizeof(void*) ? god::align_up(x, align) : x;
    }

    _p = god::cast<uint32>(p - x) + n;
    return p;
}

class StaticAlloc {
  public:
    StaticAlloc(uint32 m) : _m(m) {}
    ~StaticAlloc() = default;

    void* alloc(size_t n) {
        const size_t H = co::cache_line_size >> 1;
        return _m.alloc((uint32)n, n <= H ? sizeof(void*) : co::cache_line_size);
    }

  private:
    MemBlocks _m;
};

typedef std::function<void()> F;

class Dealloc {
  public:
    static const uint32 N = sizeof(F);
    Dealloc() : _m(8192) {}

    ~Dealloc() {
        const uint32 M = _m.blk_size() / N;
        for (uint32 i = _m.size(); i > 0; --i) {
            const uint32 m = i != _m.size() ? M : _m.pos() / N;
            for (uint32 x = m; x > 0; --x) {
                F* f = god::cast<F*>(_m[i - 1] + (x - 1) * N);
                (*f)();
                f->~F();
            }
        }
    }

    void add_destructor(F&& f) {
        const auto p = _m.alloc(N);
        new(p) F(std::forward<F>(f));
    }
    
  private:
    MemBlocks _m;
};

class Root {
  public:
    Root() : _mtx(), _sa(8192) {}
    ~Root() = default;

    template<typename T, typename... Args>
    T* make(Args&&... args) {
        void* p;
        {
            std::lock_guard<std::mutex> g(_mtx);
            p = _sa.alloc(sizeof(T));
            if (p) _da.add_destructor([p](){ ((T*)p)->~T(); });
        }
        return p ? new(p) T(std::forward<Args>(args)...) : 0;
    }

    void add_destructor(F&& f, int i) {
        std::lock_guard<std::mutex> g(_mtx);
        _dx[i].add_destructor(std::forward<F>(f));
    }

  private:
    std::mutex _mtx;
    StaticAlloc _sa; // alloc memory for GlobalAlloc and ThreadAlloc
    Dealloc _da;     // used to destruct GlobalAlloc and ThreadAlloc
    Dealloc _dx[4];  // 0: _rootic, 1: _static, 2: rootic, 3: static 
};

#if __arch64
static const uint32 B = 6;
static const uint32 g_array_size = 32;
#else
static const uint32 B = 5;
static const uint32 g_array_size = 4;
#endif

static const uint32 R = (1 << B) - 1;
static const size_t C = (size_t)1;
static const uint32 g_sb_bits = 15;            // bit size of small block
static const uint32 g_lb_bits = g_sb_bits + B; // bit size of large block
static const uint32 g_hb_bits = g_lb_bits + B; // bit size of huge block
static const size_t g_max_alloc_size = 1u << 17; // 128k

class Bitset {
  public:
    explicit Bitset(void* s) : _s((size_t*)s) {}

    void set(uint32 i) {
        _s[i >> B] |= (C << (i & R));
    }

    void unset(uint32 i) {
        _s[i >> B] &= ~(C << (i & R));
    }

    bool test_and_unset(uint32 i) {
        const size_t x = (C << (i & R));
        return god::fetch_and(&_s[i >> B], ~x) & x;
    }

    // find for a bit from MSB to LSB, starts from position @i
    int rfind(uint32 i) const {
        int n = static_cast<int>(i >> B);
        do {
            const size_t x = _s[n];
            if (x) return _find_msb(x) + (n << B);
        } while (--n >= 0);
        return -1;
    }

    void atomic_set(uint32 i) {
        atomic_or(&_s[i >> B], C << (i & R), mo_relaxed);
    }

  private:
    size_t* _s;
};

// 128M on arch64, or 32M on arch32
// manage and alloc large blocks(2M or 1M)
class HugeBlock : public co::clink {
  public:
    explicit HugeBlock(void* p) : _p((char*)p) {
        //assert(!next && !prev && _bits == 0);
    }

    void* alloc() {
        const uint32 i = _find_lsb(~_bits);
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

  private:
    char* _p; // beginning address to alloc
    size_t _bits;
    DISALLOW_COPY_AND_ASSIGN(HugeBlock);
};

inline HugeBlock* make_huge_block() {
    void* x = _vm_reserve(1u << g_hb_bits);
    if (x) {
        _vm_commit(x, 4096);
        void* p = god::align_up<(1u << g_lb_bits)>(x);
        if (p == x) p = (char*)x + (1u << g_lb_bits);
        return new (x) HugeBlock(p);
    }
    return NULL;
}

// 2M on arch64, or 1M on arch32
// manage and alloc small blocks(32K)
class LargeBlock : public co::clink {
  public:
    explicit LargeBlock(HugeBlock* parent)
        : _p((char*)this + (1u << g_sb_bits)), _parent(parent) {
        //assert(!next && !prev && _bits == 0);
    }

    void* alloc() {
        const uint32 i = _find_lsb(~_bits);
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

  private:
    char* _p; // beginning address to alloc
    size_t _bits;
    HugeBlock* _parent;
    DISALLOW_COPY_AND_ASSIGN(LargeBlock);
};

// manage huge blocks, and alloc large blocks
//   - shared by all threads
class GlobalAlloc;

// thread-local allocator
class ThreadAlloc;

// LargeAlloc is a large block, it allocates memory from 4K to 128K(64K) bytes
class LargeAlloc : public co::clink {
  public:
    static const uint32 BS_BITS = 1u << (g_lb_bits - 12);
    static const uint32 LA_SIZE = 64;
    static const uint32 MAX_BIT = BS_BITS - 1;

    explicit LargeAlloc(HugeBlock* parent, ThreadAlloc* ta)
        : _parent(parent), _ta(ta) {
        static_assert(sizeof(*this) <= LA_SIZE, "");
        static_assert((BS_BITS >> 3) <= LA_SIZE, "");
        _p = (char*)this + 4096;
        _pbs = (char*)this + LA_SIZE;
        _xpbs = (char*)this + (LA_SIZE + LA_SIZE);
        //assert(!next && !prev && _bit == 0);
    }

    // alloc n units
    void* alloc(uint32 n) {
        if (_bit + n <= MAX_BIT) {
            _bs.set(_bit);
            return _p + (god::fetch_add(&_bit, n) << 12);
        }
        return NULL;
    }

    void* try_hard_alloc(uint32 n);

    bool free(void* p) {
        int i = (int)(((char*)p - _p) >> 12);
        //CHECK(_bs.test_and_unset((uint32)i)) << "free invalid pointer: " << p;
        _bs.unset(i);
        const int r = _bs.rfind(_bit);
        return r < i ? ((_bit = r >= 0 ? i : 0) == 0) : false;
    }

    void xfree(void* p) {
        const uint32 i = (uint32)(((char*)p - _p) >> 12);
        _xbs.atomic_set(i);
    }

    void* realloc(void* p, uint32 o, uint32 n) {
        uint32 i = (uint32)(((char*)p - _p) >> 12);
        if (_bit == i + o && i + n <= MAX_BIT) {
            _bit = i + n;
            return p;
        }
        return NULL;
    }

    HugeBlock* parent() const { return _parent; }
    ThreadAlloc* talloc() const { return _ta; }

  private:
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
    DISALLOW_COPY_AND_ASSIGN(LargeAlloc);
};

void* LargeAlloc::try_hard_alloc(uint32 n) {
    size_t* const p = (size_t*)_pbs;
    size_t* const q = (size_t*)_xpbs;

    int i = _bit >> B;
    while (p[i] == 0) --i;
    size_t x = atomic_load(&q[i], mo_relaxed);
    if (x) {
        for (;;) {
            if (x) {
                atomic_and(&q[i], ~x, mo_relaxed);
                p[i] &= ~x;
                const int lsb = static_cast<int>(_find_lsb(x) + (i << B));
                const int r = _bs.rfind(_bit);
                if (r >= lsb) break;
                _bit = r >= 0 ? lsb : 0;
                if (_bit == 0) break;
            }
            if (--i < 0) break;
            x = atomic_load(&q[i], mo_relaxed);
        }
    }

    if (_bit + n <= MAX_BIT) {
        _bs.set(_bit);
        return _p + (god::fetch_add(&_bit, n) << 12);
    }
    return NULL;
}

// SmallAlloc is a small block, it allocates memory from 16 to 2K bytes
class SmallAlloc : public co::clink {
  public:
    static const uint32 BS_BITS = 1u << (g_sb_bits - 4); // 2048
    static const uint32 SA_SIZE = co::cache_line_size < 64 ? 64 : co::cache_line_size;
    static const uint32 MAX_BIT = BS_BITS - ((SA_SIZE + (BS_BITS >> 2)) >> 4);

    explicit SmallAlloc(LargeBlock* parent, ThreadAlloc* ta)
        : _bit(0), _parent(parent), _ta(ta) {
        static_assert(sizeof(*this) <= SA_SIZE, "");
        static_assert((SA_SIZE & (SA_SIZE - 1)) == 0, "");
        static_assert(co::cache_line_size <= (BS_BITS >> 3), "");
        _p = (char*)this + (SA_SIZE + (BS_BITS >> 2));
        _pbs = (char*)this + SA_SIZE;
        _xpbs = (char*)this + (SA_SIZE + (BS_BITS >> 3));
        next = prev = 0;
    }

    // alloc n units
    void* alloc(uint32 n) {
        if (_bit + n <= MAX_BIT) {
            _bs.set(_bit);
            return _p + (god::fetch_add(&_bit, n) << 4);
        }
        return NULL;
    }

    void* alloc(uint32 n, uint32 a) {
        void* p = NULL;
        const uint32 bit = (a <= (co::cache_line_size >> 4) || a <= (SA_SIZE >> 4))
            ? god::align_up(_bit, a)
            : god::align_up(_bit, a) + (god::cast<uint32>(god::align_up(_p, a << 4) - _p) >> 4);

        n = god::align_up(n, a);
        if (bit + n <= MAX_BIT) {
            _bs.set(bit);
            p = _p + (bit << 4);
            _bit = bit + n;
        }
        return p;
    }

    void* try_hard_alloc(uint32 n);

    bool free(void* p) {
        const int i = (int)(((char*)p - _p) >> 4);
        //CHECK(_bs.test_and_unset((uint32)i)) << "free invalid pointer: " << p;
        _bs.unset(i);
        const int r = _bs.rfind(_bit);
        return r < i ? ((_bit = r >= 0 ? i : 0) == 0) : false;
    }

    void xfree(void* p) {
        const uint32 i = (uint32)(((char*)p - _p) >> 4);
        _xbs.atomic_set(i);
    }

    void* realloc(void* p, uint32 o, uint32 n) {
        uint32 i = (uint32)(((char*)p - _p) >> 4);
        if (_bit == i + o && i + n <= MAX_BIT) {
            _bit = i + n;
            return p;
        }
        return NULL;
    }

    LargeBlock* parent() const { return _parent; }
    ThreadAlloc* talloc() const { return _ta; }

  private:
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
    DISALLOW_COPY_AND_ASSIGN(SmallAlloc);
};

void* SmallAlloc::try_hard_alloc(uint32 n) {
    size_t* const p = (size_t*)_pbs;
    size_t* const q = (size_t*)_xpbs;

    int i = _bit >> B;
    while (p[i] == 0) --i;
    size_t x = atomic_load(&q[i], mo_relaxed);
    if (x) {
        for (;;) {
            if (x) {
                atomic_and(&q[i], ~x, mo_relaxed);
                p[i] &= ~x;
                const int lsb = static_cast<int>(_find_lsb(x) + (i << B));
                const int r = _bs.rfind(_bit);
                if (r >= lsb) break;
                _bit = r >= 0 ? lsb : 0;
                if (_bit == 0) break;
            }
            if (--i < 0) break;
            x = atomic_load(&q[i], mo_relaxed);
        }
    }

    if (_bit + n <= MAX_BIT) {
        _bs.set(_bit);
        return _p + (god::fetch_add(&_bit, n) << 4);
    }
    return NULL;
}


class GlobalAlloc {
  public:
    GlobalAlloc() = default;
    ~GlobalAlloc();

    struct alignas(64) X {
        X() : mtx(), hb(0) {}
        std::mutex mtx;
        union {
            HugeBlock* hb;
            co::clist lhb;
        };
    };

    void* alloc(uint32 alloc_id, HugeBlock** parent);
    LargeBlock* make_large_block(uint32 alloc_id);
    LargeAlloc* make_large_alloc(uint32 alloc_id);
    void free(void* p, HugeBlock* hb, uint32 alloc_id);

  private:
    X _x[g_array_size];
};

GlobalAlloc::~GlobalAlloc() {
    for (uint32 i = 0; i < g_array_size; ++i) {
        std::lock_guard<std::mutex> g(_x[i].mtx);
        HugeBlock *h = _x[i].hb, *next;
        while (h) {
            next = (HugeBlock*) h->next;
            _vm_free(h, 1u << g_hb_bits);
            h = next;
        }
    }
}

static uint32 g_talloc_id = (uint32)-1;

class alignas(co::cache_line_size) ThreadAlloc {
  public:
    ThreadAlloc(GlobalAlloc* ga)
        : _lb(0), _la(0), _sa(0), _ga(ga), _s(16 * 1024) {
        _id = atomic_inc(&g_talloc_id, mo_relaxed);
    }
    ~ThreadAlloc() = default;

    uint32 id() const { return _id; }
    void* alloc(size_t n);
    void* alloc(size_t n, size_t align);
    void free(void* p, size_t n);
    void* realloc(void* p, size_t o, size_t n);
    void* try_realloc(void* p, size_t o, size_t n);
    void* salloc(size_t n) { return _s.alloc(n); }

  private:
    union { LargeBlock* _lb; co::clist _llb; };
    union { LargeAlloc* _la; co::clist _lla; };
    union { SmallAlloc* _sa; co::clist _lsa; };
    uint32 _id;
    GlobalAlloc* _ga;
    StaticAlloc _s; 
};


static int g_nifty_counter;
char g_root_buf[sizeof(Root)];
Root& g_root = *(Root*)g_root_buf;
static GlobalAlloc* g_ga;
__thread ThreadAlloc* g_ta;

Initializer::Initializer() {
    if (g_nifty_counter++ == 0) {
        new (&g_root) Root();
        g_ga = g_root.make<GlobalAlloc>();
    }
}

Initializer::~Initializer() {
    if (--g_nifty_counter == 0) g_root.~Root();
}

inline ThreadAlloc* talloc() {
    return g_ta ? g_ta : (g_ta = g_root.make<ThreadAlloc>(g_ga));
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
    if (p) _vm_commit(p, 1u << g_lb_bits);
    return p;
}

inline void GlobalAlloc::free(void* p, HugeBlock* hb, uint32 alloc_id) {
    _vm_decommit(p, 1u << g_lb_bits);
    auto& x = _x[alloc_id & (g_array_size - 1)];
    bool r;
    {
        std::lock_guard<std::mutex> g(x.mtx);
        r = hb->free(p) && hb != x.hb;
        if (r) x.lhb.erase(hb);
    }
    if (r) _vm_free(hb, 1u << g_hb_bits);
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

inline void* ThreadAlloc::alloc(size_t n) {
    void* p = 0;
    SmallAlloc* sa;
    if (n <= 2048) {
        const uint32 u = n > 16 ? god::nb<16>((uint32)n) : 1;
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

    } else if (n <= g_max_alloc_size) {
        const uint32 u = god::nb<4096>((uint32)n);
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
        p = ::malloc(n);
    }

  end:
    return p;
}

inline void* ThreadAlloc::alloc(size_t n, size_t align) {
    if (align < 32) align = 32;
    assert(align <= 1024 && !(align & (align - 1)));

    void* p = 0;
    SmallAlloc* sa;
    if (n <= 2048) {
        const uint32 a = (uint32)align >> 4;
        const uint32 u = n > 16 ? god::nb<16>((uint32)n) : 1;
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
        p = this->alloc(n);
    }

  end:
    return p;
}

inline void ThreadAlloc::free(void* p, size_t n) {
    if (p) {
        if (n <= 2048) {
            const auto sa = (SmallAlloc*) god::align_down<1u << g_sb_bits>(p);
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

        } else if (n <= g_max_alloc_size) {
            const auto la = (LargeAlloc*) god::align_down<1u << g_lb_bits>(p);
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
            ::free(p);
        }
    }
}

inline void* ThreadAlloc::realloc(void* p, size_t o, size_t n) {
    if (unlikely(!p)) return this->alloc(n);
    if (unlikely(o > g_max_alloc_size)) return ::realloc(p, n);
    CHECK_LT(o, n) << "realloc error, new size must be greater than old size..";

    if (o <= 2048) {
        const uint32 k = (o > 16 ? god::align_up<16>((uint32)o) : 16);
        if (n <= (size_t)k) return p;

        const auto sa = (SmallAlloc*) god::align_down<1u << g_sb_bits>(p);
        if (sa == _sa && n <= 2048) {
            const uint32 l = god::nb<16>((uint32)n);
            auto x = sa->realloc(p, k >> 4, l);
            if (x) return x;
        }

    } else {
        const uint32 k = god::align_up<4096>((uint32)o);
        if (n <= (size_t)k) return p;

        const auto la = (LargeAlloc*) god::align_down<1u << g_lb_bits>(p);
        if (la == _la && n <= g_max_alloc_size) {
            const uint32 l = god::nb<4096>((uint32)n);
            auto x = la->realloc(p, k >> 12, l);
            if (x) return x;
        }
    }

    auto x = this->alloc(n);
    if (x) { memcpy(x, p, o); this->free(p, o); }
    return x;
}

inline void* ThreadAlloc::try_realloc(void* p, size_t o, size_t n) {
    if (unlikely(!p || o > g_max_alloc_size)) return NULL;
    CHECK_LT(o, n) << "realloc error, new size must be greater than old size..";

    if (o <= 2048) {
        const uint32 k = (o > 16 ? god::align_up<16>((uint32)o) : 16);
        if (n <= (size_t)k) return p;

        const auto sa = (SmallAlloc*) god::align_down<1u << g_sb_bits>(p);
        if (sa == _sa && n <= 2048) {
            const uint32 l = god::nb<16>((uint32)n);
            return sa->realloc(p, k >> 4, l);
        }

    } else {
        const uint32 k = god::align_up<4096>((uint32)o);
        if (n <= (size_t)k) return p;

        const auto la = (LargeAlloc*) god::align_down<1u << g_lb_bits>(p);
        if (la == _la && n <= g_max_alloc_size) {
            const uint32 l = god::nb<4096>((uint32)n);
            return la->realloc(p, k >> 12, l);
        }
    }

    return NULL;
}

} // xx

void* _salloc(size_t n) {
    assert(n <= 4096);
    return xx::talloc()->salloc(n);
}

void _dealloc(std::function<void()>&& f, int x) {
    xx::g_root.add_destructor(std::forward<xx::F>(f), x);
}

#ifndef CO_USE_SYS_MALLOC
void* alloc(size_t n) {
    return xx::talloc()->alloc(n);
}

void* alloc(size_t n, size_t align) {
    return xx::talloc()->alloc(n, align);
}

void free(void* p, size_t n) {
    return xx::talloc()->free(p, n);
}

void* realloc(void* p, size_t o, size_t n) {
    return xx::talloc()->realloc(p, o, n);
}

void* try_realloc(void* p, size_t o, size_t n) {
    return xx::talloc()->try_realloc(p, o, n);
}

#else
void* alloc(size_t n) { return ::malloc(n); }
void* alloc(size_t n, size_t) { return ::malloc(n); }
void free(void* p, size_t) { ::free(p); }
void* realloc(void* p, size_t, size_t n) { return ::realloc(p, n); }
void* try_realloc(void*, size_t, size_t) { return NULL; }
#endif

void* zalloc(size_t size) {
    if (size <= xx::g_max_alloc_size) {
        auto p = co::alloc(size);
        if (p) memset(p, 0, size);
        return p;
    }
    return ::calloc(1, size);
}

char* strdup(const char* s) {
    const size_t n = strlen(s);
    char* const p = (char*) co::alloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

} // co
