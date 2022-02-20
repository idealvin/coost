#include "co/alloc.h"
#include "co/atomic.h"
#include "co/god.h"
#include "co/log.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <memoryapi.h>
#include <intrin.h>

inline void* _virtual_alloc(size_t n) {
    return VirtualAlloc(NULL, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

inline void _virtual_free(void* p, size_t n) {
    VirtualFree(p, n, MEM_RELEASE);
}

#if __arch64
inline int _find_msb(uint64 v) {
    unsigned long i;
    return _BitScanReverse64(&i, v) ? (int)i : -1;
}

#else
inline int _find_msb(uint32 v) {
    unsigned long i;
    return _BitScanReverse(&i, v) ? (int)i : -1;
}
#endif

#else
#include <sys/mman.h>

inline void* _virtual_alloc(size_t n) {
    return ::mmap(
        NULL, n, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
    );
}

inline void _virtual_free(void* p, size_t n) {
    munmap(p, n);
}

#if __arch64
inline int _find_msb(uint64 v) {
    return v ? (63 - __builtin_clzll(v)) : -1;
}

#else
inline int _find_msb(uint32 v) {
    return v ? (31 - __builtin_clz(v)) : -1;
}
#endif

#endif

namespace co {
namespace xx {

class StaticAllocator {
  public:
    static const size_t N = 16 * 1024; // block size

    StaticAllocator() : _p(0), _e(0) {}

    void* alloc(size_t n);

  private:
    char* _p;
    char* _e;
};

inline void* StaticAllocator::alloc(size_t n) {
    n = god::align_up(n, 8);
    if (_p + n <= _e) return god::fetch_add(&_p, n);

    if (n <= 1024) {
        _p = (char*) ::malloc(N); assert(_p);
        _e = _p + N;
        return god::fetch_add(&_p, n);
    }

    return ::malloc(n);
}

class MemBlock;
class MemAlloc;

class Alloc {
  public:
    Alloc();

    uint32 id() const { return _id; }

    void* alloc(size_t n);
    void free(void* p, size_t n);
    void* realloc(void* p, size_t o, size_t n);

    void* alloc_fixed(size_t n);
    void free_fixed(void* p, size_t n);

    void* alloc_static(size_t n) { return _sa.alloc(n); }

  private:
    StaticAllocator _sa;
    MemBlock* _mb_32m;
    MemBlock* _mb_1md;
    MemAlloc* _ma_1md;
    MemAlloc* _ma_32kd;
    MemBlock* _mb_1mf;
    MemAlloc* _ma_32kf;
    uint32 _id;
};

__thread Alloc* g_alloc = NULL;

inline Alloc* galloc() {
    return g_alloc ? g_alloc : (g_alloc = new Alloc());
}

class Bitset {
  public:
  #if __arch64
    typedef uint64 type;
    static const int B = 6;
  #else
    typedef uint32 type;
    static const int B = 5;
  #endif

    static const int R = (1 << B) - 1;
    static const type C = (type)1;

    explicit Bitset(void* s) : _s((type*)s) {}

    void set(uint32 i) {
        atomic_or(&_s[i >> B], C << (i & R));
    }

    bool test_and_unset(uint32 i) {
        const type x = (C << (i & R));
        return atomic_fetch_and(&_s[i >> B], ~x) & x;
    }

    // find for a set bit from MSB to LSB, starts from position @i
    int rfind(uint32 i) const {
        int n = static_cast<int>(i >> B);
        do {
            int x = _find_msb(_s[n]);
            if (x >= 0) return x + (n << B);
        } while (--n >= 0);
        return -1;
    }

  private:
    type* _s;
};

// a block contains 32 sub blocks
class MemBlock {
  public:
    MemBlock(MemBlock* parent, char* b, char* p, uint32 sub_bits)
        : _parent(parent), _b(b), _p(p), _bits(0), _max_bit(31), _sub_bits(sub_bits) {
        _size = 32 * (1u << sub_bits);
    }

    MemBlock* make_mem_block(uint32 sub_bits);
    MemAlloc* make_mem_alloc(uint32 unit_bits, uint32 bs_bits, uint32 alloc_id);

    void* alloc();
    void free(void* p);

  private:
    void _set_bit(uint32 i) { atomic_or(&_bits, 1u << i); }
    uint32 _unset_bit(uint32 i) { return atomic_and(&_bits, ~(1u << i)); }
    uint32 _find_lsb(); // find least significant bit

  private:
    MemBlock* _parent;
    char* _b; // beginning address of this block 
    char* _p; // beginning address to alloc
    uint32 _bits;
    uint32 _max_bit;
    uint32 _sub_bits; // bit size of a sub block
    uint32 _size;     // size of this block
    DISALLOW_COPY_AND_ASSIGN(MemBlock);
};

class MemAlloc {
  public:
    MemAlloc(MemBlock* mb, uint32 unit_bits, uint32 bs_bits, uint32 alloc_id);

    // alloc n units
    void* alloc(uint32 n);
    void free(void* p);
    void* realloc(void* p, uint32 o, uint32 n);

  private:
    MemBlock* _mb;
    char* _p; // beginning address to alloc
    union {
        Bitset _bs;
        void* _dummy;
    };
    uint32 _cur_bit;
    uint32 _max_bit;
    uint32 _count;
    uint32 _unit_bits; // bit size of an unit
    uint32 _alloc_id;  // allocator id
    uint32 _size;      // size of this block
    DISALLOW_COPY_AND_ASSIGN(MemAlloc);
};

inline MemBlock* make_mem_block(uint32 sub_bits, MemBlock* parent=0, char* b=0) {
    const uint32 sub_size = 1u << sub_bits;
    if (!b) b = (char*) _virtual_alloc(32 * sub_size);
    if (b) {
        char* p = god::align_up(b, sub_size);
        if (p == b) p += sub_size; // sub_size is 32k or 1m
        const size_t off = god::align_up(sizeof(MemBlock), 16);
        return new (p - off) MemBlock(parent, b, p, sub_bits);
    }
    return NULL;
}

inline uint32 MemBlock::_find_lsb() {
    const uint32 x = _bits;
  #ifdef _WIN32
    unsigned long r;
    return _BitScanForward(&r, ~x) ? r : 32;
  #else
    const uint32 r = __builtin_ffs(~x);
    return r > 0 ? r - 1 : 32;
  #endif
}

inline MemBlock* MemBlock::make_mem_block(uint32 sub_bits) {
    const uint32 i = this->_find_lsb();
    if (i < _max_bit) {
        this->_set_bit(i);
        return xx::make_mem_block(sub_bits, this, _p + (i << _sub_bits));
    }
    atomic_swap(&_max_bit, 0); // need not this block anymore
    return NULL;
}

inline MemAlloc* MemBlock::make_mem_alloc(uint32 unit_bits, uint32 bs_bits, uint32 alloc_id) {
    const uint32 i = this->_find_lsb();
    if (i < _max_bit) {
        this->_set_bit(i);
        return new (_p + (i << _sub_bits)) MemAlloc(this, unit_bits, bs_bits, alloc_id);
    }
    atomic_swap(&_max_bit, 0); // need not this block anymore
    return NULL;
}

inline void MemBlock::free(void* p) {
    const uint32 i = (uint32)(((char*)p - _p) >> _sub_bits);
    if (this->_unset_bit(i) == 0 && _max_bit == 0) {
        _parent ? _parent->free(_b) : _virtual_free(_b, _size);
    }
}

MemAlloc::MemAlloc(MemBlock* mb, uint32 unit_bits, uint32 bs_bits, uint32 alloc_id)
    : _mb(mb), _cur_bit(0), _count(0), _unit_bits(unit_bits), _alloc_id(alloc_id) {
    const uint32 unit_size = 1u << unit_bits;
    const uint32 n = god::align_up((uint32)sizeof(*this), 16);
    uint32 reserved_size = god::align_up(n + (bs_bits >> 3), unit_size);
    _max_bit = bs_bits - (reserved_size >> _unit_bits);
    _size = bs_bits * unit_size;
    _p = (char*)this + reserved_size;
    new (&_bs) Bitset((char*)this + n);
}

inline void* MemAlloc::alloc(uint32 n) {
    (_count == 0 && _cur_bit != 0) ? (void)(_cur_bit = 0) : (void)0;
    if (_cur_bit + n <= _max_bit) {
        _bs.set(_cur_bit);
        atomic_inc(&_count);
        return _p + (god::fetch_add(&_cur_bit, n) << _unit_bits);
    }
    atomic_add(&_cur_bit, n); // now _cur_bit > _max_bit
    return NULL;
}

inline void MemAlloc::free(void* p) {
    int i = (int)(((char*)p - _p) >> _unit_bits);
    CHECK(_bs.test_and_unset(i)) << "free invalid pointer: " << p;

    if (_cur_bit <= _max_bit) {
        atomic_dec(&_count);
        if (galloc()->id() == _alloc_id) { /* free in the same thread as alloc */
            const int r = _bs.rfind(_cur_bit);
            if (r < i) {
                _cur_bit = r >= 0 ? i : 0;
            }
        }

    } else {
        if (atomic_dec(&_count) == 0) _mb->free(this);
    }
}

inline void* MemAlloc::realloc(void* p, uint32 o, uint32 n) {
    uint32 i = (uint32)(((char*)p - _p) >> _unit_bits);
    if (_cur_bit == i + o && i + n <= _max_bit) {
        _cur_bit = i + n;
        return p;
    }
    return NULL;
}

inline uint32 _pow2_align(uint32 n) {
  #ifdef _WIN32
    unsigned long index;
    _BitScanReverse(&index, n - 1);
    return 1u << (index + 1);
  #else
    return 1u << (32 - __builtin_clz(n - 1));
  #endif
}

void* Alloc::alloc(size_t n) {
    void* p = 0;
    if (n <= 2 * 1024) {
        const uint32 u = (n > 16 ? _pow2_align(n) : 16) >> 4;
        if (_ma_32kd && (p = _ma_32kd->alloc(u))) goto _end;

        if (_mb_1md && (_ma_32kd = _mb_1md->make_mem_alloc(4, 2048, _id))) {
            p = _ma_32kd->alloc(u);
            goto _end;
        }

        if (_mb_32m && (_mb_1md = _mb_32m->make_mem_block(15))) {
            _ma_32kd = _mb_1md->make_mem_alloc(4, 2048, _id);
            p = _ma_32kd->alloc(u);
            goto _end;
        }

        _mb_32m = xx::make_mem_block(20);
        if (_mb_32m) {
            _mb_1md = _mb_32m->make_mem_block(15);
            _ma_32kd = _mb_1md->make_mem_alloc(4, 2048, _id);
            p = _ma_32kd->alloc(u);
            goto _end;
        }

    } else {
        const size_t i = (n - 1) >> 16;
        switch (i) {
          case 0:
            goto _2k_64k_beg;
          case 1:
            p = ::malloc(128 * 1024);
            goto _end;
          default:
            p = ::malloc(n);
            goto _end;
        }

      _2k_64k_beg:
        const uint32 u = _pow2_align(n) >> 12;
        if (_ma_1md && (p = _ma_1md->alloc(u))) goto _end;

        if (_mb_32m && (_ma_1md = _mb_32m->make_mem_alloc(12, 256, _id))) {
            p = _ma_1md->alloc(u);
            goto _end;
        }

        _mb_32m = xx::make_mem_block(20);
        if (_mb_32m) {
            _ma_1md = _mb_32m->make_mem_alloc(12, 256, _id);
            p = _ma_1md->alloc(u);
            goto _end;
        }
    }

  _end:
    return p;
}

inline void Alloc::free(void* p, size_t n) {
    if (unlikely(!p)) return;
    if (n <= 2048) {
        auto ma = (MemAlloc*) god::align_down(p, 32 * 1024);
        ma->free(p);
    } else if (n <= 64 * 1024) {
        auto ma = (MemAlloc*) god::align_down(p, 1024 * 1024);
        ma->free(p);
    } else {
        ::free(p);
    }
}

void* Alloc::realloc(void* p, size_t o, size_t n) {
    if (unlikely(!p)) return this->alloc(n);
    CHECK_LT(o, n) << "realloc error, new size must be greater than old size..";

    if (o <= 2048) {
        const uint32 k = (o > 16 ? _pow2_align((uint32)o) : 16);
        if (n <= (size_t)k) return p;

        auto ma = (MemAlloc*) god::align_down(p, 32 * 1024);
        if (ma == _ma_32kd && n <= 2048) {
            const uint32 l = _pow2_align((uint32)n);
            auto x = ma->realloc(p, k >> 4, l >> 4);
            if (x) return x;
        }

        auto x = this->alloc(n);
        if (x) { memcpy(x, p, o); ma->free(p); }
        return x;

    } else if (o <= 64 * 1024) {
        const uint32 k = _pow2_align((uint32)o);
        if (n <= (size_t)k) return p;

        auto ma = (MemAlloc*) god::align_down(p, 1024 * 1024);
        if (ma == _ma_1md && n <= 64 * 1024) {
            const uint32 l = _pow2_align((uint32)n);
            auto x = ma->realloc(p, k >> 12, l >> 12);
            if (x) return x;
        }

        auto x = this->alloc(n);
        if (x) { memcpy(x, p, o); ma->free(p); }
        return x;

    } else if (o <= 128 * 1024 && n <= 128 * 1024) {
        return p;
    }

    return ::realloc(p, n);

}

inline void* Alloc::alloc_fixed(size_t n) {
    void* p = 0;
    if (n <= 2048) {
        const uint32 u = n > 0 ? (god::align_up((uint32)n, 16) >> 4) : 1;
        if (_ma_32kf && (p = _ma_32kf->alloc(u))) goto _end;

        if (_mb_1mf && (_ma_32kf = _mb_1mf->make_mem_alloc(4, 2048, _id)) ) {
            p = _ma_32kf->alloc(u);
            goto _end;
        }

        if (_mb_32m && (_mb_1mf = _mb_32m->make_mem_block(15))) {
            _ma_32kf = _mb_1mf->make_mem_alloc(4, 2048, _id);
            p = _ma_32kf->alloc(u);
            goto _end;
        }

        _mb_32m = xx::make_mem_block(20);
        if (_mb_32m) {
            _mb_1mf = _mb_32m->make_mem_block(15);
            _ma_32kf = _mb_1mf->make_mem_alloc(4, 2048, _id);
            p = _ma_32kf->alloc(u);
            goto _end;
        }

    } else {
        p = this->alloc(n);
    }

  _end:
    return p;
}

inline void Alloc::free_fixed(void* p, size_t n) {
    if (unlikely(!p)) return;
    if (n <= 2048) {
        auto ma = (MemAlloc*) god::align_down(p, 32 * 1024);
        ma->free(p);
    } else {
        this->free(p, n);
    }
}

static uint32 g_alloc_id = (uint32)-1;

Alloc::Alloc()
    : _mb_32m(0), _mb_1md(0), _ma_1md(0), _ma_32kd(0),
      _mb_1mf(0), _ma_32kf(0) {
    _id = atomic_inc(&g_alloc_id);
}
} // xx

void* alloc_static(size_t size) {
    return xx::galloc()->alloc_static(size);
}

void* alloc_fixed(size_t size) {
    return xx::galloc()->alloc_fixed(size);
}

void free_fixed(void* p, size_t size) {
    return xx::galloc()->free_fixed(p, size);
}

void* alloc(size_t size) {
    return xx::galloc()->alloc(size);
}

void free(void* p, size_t size) {
    return xx::galloc()->free(p, size);
}

void* realloc(void* p, size_t old_size, size_t new_size) {
    return xx::galloc()->realloc(p, old_size, new_size);
}

} // co
