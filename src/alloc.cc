#include "co/alloc.h"
#include "co/god.h"
#include "co/stl/vector.h"

namespace co {
namespace xx {

class StaticAllocator {
  public:
    static const size_t N = 8192;

    struct Block {
        char* p;
        char* e;
    };

    StaticAllocator()
        : _mem(8), _blk(0) {
    }

    // no need to free the memory
    ~StaticAllocator() = default;

    void* alloc(size_t n);

  private:
    co::vector<Block> _mem;
    size_t _blk; // the current block
};

inline void* StaticAllocator::alloc(size_t n) {
    n = god::b8(n) << 3;
    if (_blk < _mem.size() && _mem[_blk].p + n <= _mem[_blk].e) {
        void* const x = _mem[_blk].p;
        _mem[_blk].p += n;
        return x;
    } else {
        _blk = _mem.size();
        _mem.resize(_mem.size() + 1);
        const size_t size = n < N ? N : n;
        char* const x = (char*)::malloc(size);
        _mem[_blk].p = x + n;
        _mem[_blk].e = x + size;
        return x;
    }
}

class Allocator {
  public:
    Allocator() = default;
    ~Allocator() = default;

    void* alloc_static(size_t n) {
        return _sa.alloc(n);
    }

  private:
    StaticAllocator _sa;
};

__thread Allocator* gAlloc = NULL;

} // xx

void* alloc_static(size_t size) {
    if (unlikely(xx::gAlloc == NULL)) xx::gAlloc = new xx::Allocator();
    return xx::gAlloc->alloc_static(size);
}

} // co
