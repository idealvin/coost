#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4200)
#endif

#include "co/mem.h"

namespace co {

struct Buffer {
    struct H {
        uint32 cap;
        uint32 size;
        char p[];
    };

    Buffer() = delete;
    ~Buffer() = delete;

    const char* data() const noexcept { return _h ? _h->p : 0; }
    uint32 size() const noexcept { return _h ? _h->size : 0; }
    uint32 capacity() const noexcept { return _h ? _h->cap : 0; }
    void clear() noexcept { if (_h) _h->size = 0; }

    void reset() {
        if (_h) {
            co::free(_h, _h->cap + sizeof(H));
            _h = 0;
        }
    }

    void append(const void* p, size_t size) {
        const uint32 n = (uint32)size;
        if (_h) goto _1;

        _h = (H*) co::alloc(size + sizeof(H));
        runtime_assert(_h);
        _h->cap = n;
        _h->size = 0;
        goto _2;

    _1:
        if (_h->size + n <= _h->cap) goto _2;

        {
            const uint32 o = _h->cap;
            _h->cap += (o >> 1) + n;
            _h = (H*) co::realloc(_h, o + sizeof(H), _h->cap + sizeof(H));
            runtime_assert(_h);
        }

    _2:
        ::memcpy(_h->p + _h->size, p, n);
        _h->size += n;
    }

    H* _h;
};

} // co
