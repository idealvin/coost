#include "co/mem.h"
#include "co/atomic.h"
#include "co/god.h"
#include "co/log.h"
#include "co/vector.h"
#include <mutex>

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

inline uint32 _find_lsb(size_t x) {
    unsigned long r;
    return _BitScanForward64(&r, x) ? r : 64;
}

#else
inline int _find_msb(size_t x) { /* x != 0 */
    unsigned long i;
    _BitScanReverse(&i, x);
    return (int)i;
}

inline uint32 _find_lsb(size_t x) {
    unsigned long r;
    return _BitScanForward(&r, x) ? r : 32;
}
#endif

inline uint32 _pow2_align(uint32 n) {
    unsigned long r;
    _BitScanReverse(&r, n - 1);
    return 1u << (r + 1);
}

#else
#include <sys/mman.h>

inline void* _vm_reserve(size_t n) {
    return ::mmap(
        NULL, n, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0
    );
}

inline void _vm_commit(void* p, size_t n) {
    void* x = ::mmap(
        p, n, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0
    );
    assert(x == p); (void)x;
}

inline void _vm_decommit(void* p, size_t n) {
    ::mmap(
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

inline uint32 _find_lsb(size_t x) {
    const uint32 r = __builtin_ffsll(x);
    return r > 0 ? r - 1 : 64;
}

#else
inline int _find_msb(size_t v) { /* x != 0 */
    return 31 - __builtin_clz(v);
}

inline uint32 _find_lsb(size_t x) {
    const uint32 r = __builtin_ffs(x);
    return r > 0 ? r - 1 : 32;
}
#endif

inline uint32 _pow2_align(uint32 n) {
    return 1u << (32 - __builtin_clz(n - 1));
}

#endif


namespace co {
namespace xx {

class StaticAllocator {
  public:
    static const size_t N = 64 * 1024; // block size

    StaticAllocator() : _p(0), _e(0) {}

    void* alloc(size_t n);

  private:
    char* _p;
    char* _e;
};

inline void* StaticAllocator::alloc(size_t n) {
    n = god::align_up(n, 8);
    if (_p + n <= _e) return god::fetch_add(&_p, n);

    if (n <= 4096) {
        _p = (char*) ::malloc(N); assert(_p);
        _e = _p + N;
        return god::fetch_add(&_p, n);
    }

    return ::malloc(n);
}


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
static const size_t g_max_alloc_size = 1u << (g_lb_bits - 4);

class Bitset {
  public:
    explicit Bitset(void* s) : _s((size_t*)s) {}

    void set(uint32 i) {
        _s[i >> B] |= (C << (i & R));
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

  private:
    size_t* _s;
};

// 128M on arch64, or 32M on arch32
// manage and alloc large blocks(2M or 1M)
class HugeBlock;

// 2M on arch64, or 1M on arch32
// manage and alloc small blocks(32K)
class LargeBlock;

// LargeAlloc is a large block, it allocates memory from 4K to 128K(64K) bytes
class LargeAlloc;

// SmallAlloc is a small block, it allocates memory from 16 to 2K bytes
class SmallAlloc;

// manage huge blocks, and alloc large blocks
//   - shared by all threads
class GlobalAlloc;

// thread-local allocator
class ThreadAlloc;

class HugeBlock {
  public:
    explicit HugeBlock(void* p) : _p((char*)p), _bits(0) {}

    void* alloc(); // alloc a sub block
    bool free(void* p);

  private:
    HugeBlock* _next;
    HugeBlock* _prev;
    char* _p; // beginning address to alloc
    size_t _bits;
    DISALLOW_COPY_AND_ASSIGN(HugeBlock);
};

class LargeBlock {
  public:
    explicit LargeBlock(HugeBlock* parent)
        : _parent(parent), _p((char*)this + (1u << g_sb_bits)), _bits(0) {
    }

    void* alloc(); // alloc a sub block
    bool free(void* p);
    SmallAlloc* make_small_alloc();
    HugeBlock* parent() const { return _parent; }

  private:
    LargeBlock* _next;
    LargeBlock* _prev;
    HugeBlock* _parent;
    char* _p; // beginning address to alloc
    size_t _bits;
    DISALLOW_COPY_AND_ASSIGN(LargeBlock);
};

class LargeAlloc {
  public:
    explicit LargeAlloc(HugeBlock* parent);

    // alloc n units
    void* alloc(uint32 n);
    bool free(void* p);
    void* realloc(void* p, uint32 o, uint32 n);

    HugeBlock* parent() const { return _parent; }
    ThreadAlloc* thread_alloc() const { return _ta; }

  private:
    LargeAlloc* _next;
    LargeAlloc* _prev;
    HugeBlock* _parent;
    ThreadAlloc* _ta;
    char* _p; // beginning address to alloc
    union {
        Bitset _bs;
        char* _pbs;
    };
    uint32 _cur_bit;
    uint32 _max_bit;
    DISALLOW_COPY_AND_ASSIGN(LargeAlloc);
};

class SmallAlloc {
  public:
    explicit SmallAlloc(LargeBlock* parent);

    // alloc n units
    void* alloc(uint32 n);
    bool free(void* p);
    void* realloc(void* p, uint32 o, uint32 n);

    LargeBlock* parent() const { return _parent; }
    ThreadAlloc* thread_alloc() const { return _ta; }

  private:
    LargeBlock* _parent;
    ThreadAlloc* _ta;
    char* _p; // beginning address to alloc
    union {
        Bitset _bs;
        char* _pbs;
    };
    uint32 _cur_bit;
    uint32 _max_bit;
    DISALLOW_COPY_AND_ASSIGN(SmallAlloc);
};

class GlobalAlloc {
  public:
    struct _X {
        _X() : mtx(), hb(0) {}
        std::mutex mtx;
        HugeBlock* hb;
    };

    void* alloc(uint32 alloc_id, HugeBlock** parent);
    LargeBlock* make_large_block(uint32 alloc_id);
    LargeAlloc* make_large_alloc(uint32 alloc_id);
    void free(void* p, HugeBlock* hb, uint32 alloc_id);

  private:
    _X _x[g_array_size];
};

class ThreadAlloc {
  public:
    ThreadAlloc()
        : _lb(0), _la(0), _sad(0), _saf(0), _xlock(false), _has_xptr(false),
          _xptrs(4096), _xswap(4096) {
        static uint32 g_alloc_id = (uint32)-1;
        _id = atomic_inc(&g_alloc_id, mo_relaxed);
    }

    void* static_alloc(size_t n) { return _sa.alloc(n); }
    void* alloc(size_t n);
    void free(void* p, size_t n);
    void* realloc(void* p, size_t o, size_t n);

    void* fixed_alloc(size_t n);
    void xfree(void* p, size_t n);
    void try_free_xptrs();

    struct xptr_t {
        xptr_t() = default;
        xptr_t(void* p, size_t n) : p(p), n(n) {}
        void* p;
        size_t n;
    };

  private:
    StaticAllocator _sa;
    LargeBlock* _lb;
    LargeAlloc* _la;
    SmallAlloc* _sad;
    SmallAlloc* _saf;
    uint32 _id;
    bool _xlock;
    bool _has_xptr;
    co::vector<xptr_t, co::system_allocator> _xptrs;
    co::vector<xptr_t, co::system_allocator> _xswap;
};


__thread ThreadAlloc* g_thread_alloc = NULL;

inline GlobalAlloc* galloc() {
    static GlobalAlloc* ga = new GlobalAlloc();
    return ga;
}

inline ThreadAlloc* thread_alloc() {
    return g_thread_alloc ? g_thread_alloc : (g_thread_alloc = new ThreadAlloc());
}


struct DoubleLink {
    DoubleLink* next;
    DoubleLink* prev;
};

typedef DoubleLink* list_t;

inline void list_push_front(list_t& l, DoubleLink* node) {
    if (l) {
        node->next = l;
        node->prev = l->prev;
        l->prev = node;
        l = node;
    } else {
        node->next = NULL;
        node->prev = node;
        l = node;
    }
}

// move heading node to the back
inline void list_move_head_back(list_t& l) {
    auto head = l->next;
    l->prev->next = l;
    l->next = NULL;
    l = head;
}

// erase non-heading node
inline void list_erase(list_t& l, DoubleLink* node) {
    node->prev->next = node->next;
    DoubleLink* x = node->next ? node->next : l;
    x->prev = node->prev;
}


inline void* HugeBlock::alloc() {
    char* p = NULL;
    const uint32 i = _find_lsb(~_bits);
    if (i < R) {
        _bits |= (C << i);
        p = _p + (((size_t)i) << g_lb_bits);
    }
    return p;
}

inline bool HugeBlock::free(void* p) {
    const uint32 i = (uint32)(((char*)p - _p) >> g_lb_bits);
    _bits &= ~(C << i);
    return _bits == 0;
}

inline HugeBlock* make_huge_block() {
    void* x = _vm_reserve(1u << g_hb_bits);
    if (x) {
        _vm_commit(x, 4096);
        void* p = god::align_up(x, 1u << g_lb_bits);
        if (p == x) p = (char*)x + (1u << g_lb_bits);
        return new (x) HugeBlock(p);
    }
    return NULL;
}


inline void* GlobalAlloc::alloc(uint32 alloc_id, HugeBlock** parent) {
    void* p = NULL;
    auto& x = _x[alloc_id & (g_array_size - 1)];

    do {
        std::lock_guard<std::mutex> g(x.mtx);
        if (x.hb && (p = x.hb->alloc())) {
            *parent = x.hb;
            break;
        }

        auto& l = *(DoubleLink**)&x.hb;
        if (l && l->next) {
            list_move_head_back(l);
            if ((p = x.hb->alloc())) {
                *parent = x.hb;
                break;
            }
        }

        auto hb = make_huge_block();
        if (hb) {
            list_push_front(l, (DoubleLink*)hb);
            p = hb->alloc();
            *parent = hb;
        }
    } while (0);

    if (p) _vm_commit(p, 1u << g_lb_bits);
    return p;
}

inline LargeBlock* GlobalAlloc::make_large_block(uint32 alloc_id) {
    HugeBlock* parent;
    auto p = this->alloc(alloc_id, &parent);
    return p ? new (p) LargeBlock(parent) : NULL;
}

inline LargeAlloc* GlobalAlloc::make_large_alloc(uint32 alloc_id) {
    HugeBlock* parent;
    auto p = this->alloc(alloc_id, &parent);
    return p ? new (p) LargeAlloc(parent) : NULL;
}

inline void GlobalAlloc::free(void* p, HugeBlock* hb, uint32 alloc_id) {
    _vm_decommit(p, 1u << g_lb_bits);
    auto& x = _x[alloc_id & (g_array_size - 1)];
    bool r;
    {
        std::lock_guard<std::mutex> g(x.mtx);
        r = hb->free(p) && hb != x.hb;
        if (r) list_erase(*(DoubleLink**)&x.hb, (DoubleLink*)hb);
    }
    if (r) _vm_free(hb, 1u << g_hb_bits);
}


inline void* LargeBlock::alloc() {
    void* p = NULL;
    const uint32 i = _find_lsb(~_bits);
    if (i < R) {
        _bits |= (C << i);
        p = _p + (((size_t)i) << g_sb_bits);
    }
    return p;
}

inline bool LargeBlock::free(void* p) {
    const uint32 i = (uint32)(((char*)p - _p) >> g_sb_bits);
    _bits &= ~(C << i);
    return _bits == 0;
}

inline SmallAlloc* LargeBlock::make_small_alloc() {
    auto x = this->alloc();
    return x ? new (x) SmallAlloc(this) : NULL;
}


LargeAlloc::LargeAlloc(HugeBlock* parent)
    : _parent(parent), _ta(g_thread_alloc), _cur_bit(0) {
    _max_bit = (1u << (g_lb_bits - 12)) - 1;
    _p = (char*)this + 4096;
    _pbs = (char*)this + god::align_up((uint32)sizeof(*this), 16);
}

inline void* LargeAlloc::alloc(uint32 n) {
    if (_cur_bit + n <= _max_bit) {
        _bs.set(_cur_bit);
        return _p + (god::fetch_add(&_cur_bit, n) << 12);
    }
    return NULL;
}

inline bool LargeAlloc::free(void* p) {
    int i = (int)(((char*)p - _p) >> 12);
    CHECK(_bs.test_and_unset((uint32)i)) << "free invalid pointer: " << p;

    const int r = _bs.rfind(_cur_bit);
    if (r < i) _cur_bit = r >= 0 ? i : 0;
    return _cur_bit == 0;
}

inline void* LargeAlloc::realloc(void* p, uint32 o, uint32 n) {
    uint32 i = (uint32)(((char*)p - _p) >> 12);
    if (_cur_bit == i + o && i + n <= _max_bit) {
        _cur_bit = i + n;
        return p;
    }
    return NULL;
}


SmallAlloc::SmallAlloc(LargeBlock* parent)
    : _parent(parent), _ta(g_thread_alloc), _cur_bit(0) {
    const uint32 bs_bits = (1u << (g_sb_bits - 4)); // 2048
    const uint32 n = god::align_up((uint32)sizeof(*this), 16);
    const uint32 reserved_size = n + (bs_bits >> 3); // n + 256
    _max_bit = bs_bits - (reserved_size >> 4);
    _p = (char*)this + reserved_size;
    _pbs = (char*)this + n;
}

inline void* SmallAlloc::alloc(uint32 n) {
    if (_cur_bit + n <= _max_bit) {
        _bs.set(_cur_bit);
        return _p + (god::fetch_add(&_cur_bit, n) << 4);
    }
    return NULL;
}

inline bool SmallAlloc::free(void* p) {
    int i = (int)(((char*)p - _p) >> 4);
    CHECK(_bs.test_and_unset((uint32)i)) << "free invalid pointer: " << p;

    const int r = _bs.rfind(_cur_bit);
    if (r < i) _cur_bit = r >= 0 ? i : 0;
    return _cur_bit == 0;
}

inline void* SmallAlloc::realloc(void* p, uint32 o, uint32 n) {
    uint32 i = (uint32)(((char*)p - _p) >> 4);
    if (_cur_bit == i + o && i + n <= _max_bit) {
        _cur_bit = i + n;
        return p;
    }
    return NULL;
}


inline void ThreadAlloc::xfree(void* p, size_t n) {
    while (atomic_load(&_xlock, mo_relaxed) || atomic_swap(&_xlock, true, mo_acquire));
    _xptrs.push_back(xptr_t(p, n));
    if (!_has_xptr) atomic_store(&_has_xptr, true, mo_relaxed);
    atomic_store(&_xlock, false, mo_release);
}

inline void ThreadAlloc::try_free_xptrs() {
    if (atomic_swap(&_xlock, true, mo_acquire) == false) {
        _xswap.swap(_xptrs);
        atomic_store(&_has_xptr, false, mo_relaxed);
        atomic_store(&_xlock, false, mo_release);
        for (size_t i = 0; i < _xswap.size(); ++i) {
            this->free(_xswap[i].p, _xswap[i].n);
        }
        _xswap.clear();
    }
}

void* ThreadAlloc::alloc(size_t n) {
    if (_has_xptr) this->try_free_xptrs();

    void* p = 0;
    if (n <= 2048) {
        const uint32 u = n > 16 ? (_pow2_align((uint32)n) >> 4) : 1;
        if (_sad && (p = _sad->alloc(u))) goto _end;

        if (_lb && (_sad = _lb->make_small_alloc())) {
            p = _sad->alloc(u);
            goto _end;
        }

        {
            auto& l = *(DoubleLink**)&_lb;
            if (l && l->next) {
                list_move_head_back(l);
                if ((_sad = _lb->make_small_alloc())) {
                    p = _sad->alloc(u);
                    goto _end;
                }
            }

            auto lb = galloc()->make_large_block(_id);
            if (lb) {
                list_push_front(l, (DoubleLink*)lb);
                _sad = lb->make_small_alloc();
                p = _sad->alloc(u);
            }
            goto _end;
        }

    } else if (n <= g_max_alloc_size) {
        const uint32 u = _pow2_align((uint32)n) >> 12;
        if (_la && (p = _la->alloc(u))) goto _end;

        {
            auto& l = *(DoubleLink**)&_la;
            if (l && l->next) {
                list_move_head_back(l);
                if ((p = _la->alloc(u))) goto _end;
            }

            auto la = galloc()->make_large_alloc(_id);
            if (la) {
                list_push_front(l, (DoubleLink*)la);
                p = la->alloc(u);
            }
            goto _end;
        }

    } else {
        p = ::malloc(n);
    }

  _end:
    return p;
}

inline void ThreadAlloc::free(void* p, size_t n) {
    if (p) {
        if (n <= 2048) {
            auto sa = (SmallAlloc*) god::align_down(p, 1u << g_sb_bits);
            auto ta = sa->thread_alloc();
            if (ta == this) {
                if (sa->free(p) && sa != _sad && sa != _saf) {
                    auto lb = sa->parent();
                    if (lb->free(sa) && lb != _lb) {
                        list_erase(*(DoubleLink**)&_lb, (DoubleLink*)lb);
                        galloc()->free(lb, lb->parent(), _id);
                    }
                }
            } else {
                ta->xfree(p, n);
            }

        } else if (n <= g_max_alloc_size) {
            auto la = (LargeAlloc*) god::align_down(p, 1u << g_lb_bits);
            auto ta = la->thread_alloc();
            if (ta == this) {
                if (la->free(p) && la != _la) {
                    list_erase(*(DoubleLink**)&_la, (DoubleLink*)la);
                    galloc()->free(la, la->parent(), _id);
                }
            } else {
                ta->xfree(p, n);
            }

        } else {
            ::free(p);
        }
    }
}

void* ThreadAlloc::realloc(void* p, size_t o, size_t n) {
    if (unlikely(!p)) return this->alloc(n);
    if (unlikely(o > g_max_alloc_size)) return ::realloc(p, n);
    CHECK_LT(o, n) << "realloc error, new size must be greater than old size..";

    if (o <= 2048) {
        const uint32 k = (o > 16 ? _pow2_align((uint32)o) : 16);
        if (n <= (size_t)k) return p;

        auto sa = (SmallAlloc*) god::align_down(p, 1u << g_sb_bits);
        if (sa == _sad && n <= 2048) {
            const uint32 l = _pow2_align((uint32)n);
            auto x = sa->realloc(p, k >> 4, l >> 4);
            if (x) return x;
        }

    } else {
        const uint32 k = _pow2_align((uint32)o);
        if (n <= (size_t)k) return p;

        auto la = (LargeAlloc*) god::align_down(p, 1u << g_lb_bits);
        if (la == _la && n <= g_max_alloc_size) {
            const uint32 l = _pow2_align((uint32)n);
            auto x = la->realloc(p, k >> 12, l >> 12);
            if (x) return x;
        }
    }

    auto x = this->alloc(n);
    if (x) { memcpy(x, p, o); this->free(p, o); }
    return x;
}

inline void* ThreadAlloc::fixed_alloc(size_t n) {
    if (_has_xptr) this->try_free_xptrs();

    void* p = 0;
    if (n <= 2048) {
        const uint32 u = n > 16 ? (god::align_up((uint32)n, 16) >> 4) : 1;
        if (_saf && (p = _saf->alloc(u))) goto _end;

        if (_lb && (_saf = _lb->make_small_alloc())) {
            p = _saf->alloc(u);
            goto _end;
        }

        {
            auto& l = *(DoubleLink**)&_lb;
            if (l && l->next) {
                list_move_head_back(l);
                if ((_saf = _lb->make_small_alloc())) {
                    p = _saf->alloc(u);
                    goto _end;
                }
            }

            auto lb = galloc()->make_large_block(_id);
            if (lb) {
                list_push_front(l, (DoubleLink*)lb);
                _saf = lb->make_small_alloc();
                p = _saf->alloc(u);
            }
            goto _end;
        }

    } else {
        p = this->alloc(n);
    }

  _end:
    return p;
}

} // xx

#ifndef CO_USE_SYS_MALLOC
void* static_alloc(size_t n) {
    return xx::thread_alloc()->static_alloc(n);
}

void* fixed_alloc(size_t n) {
    return xx::thread_alloc()->fixed_alloc(n);
}

void* alloc(size_t n) {
    return xx::thread_alloc()->alloc(n);
}

void free(void* p, size_t n) {
    return xx::thread_alloc()->free(p, n);
}

void* realloc(void* p, size_t o, size_t n) {
    return xx::thread_alloc()->realloc(p, o, n);
}

#else
void* static_alloc(size_t n) { return ::malloc(n); }
void* fixed_alloc(size_t n) { return ::malloc(n); }
void* alloc(size_t n) { return ::malloc(n); }
void free(void* p, size_t) { ::free(p); }
void* realloc(void* p, size_t, size_t n) { return ::realloc(p, n); }
#endif

void* zalloc(size_t size) {
    auto p = co::alloc(size);
    if (p) memset(p, 0, size);
    return p;
}

void* fixed_zalloc(size_t size) {
    auto p = co::fixed_alloc(size);
    if (p) memset(p, 0, size);
    return p;
}

} // co
