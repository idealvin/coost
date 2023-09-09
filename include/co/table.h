#pragma once

#include <assert.h>
#include <stdlib.h>
#include <mutex>

namespace co {

// A fixed-size table.
//   - The internal memory is zero-cleared at initialization.
//   - e.g.
//     co::table<int> t(x, y); // X*Y elements can be stored in
//                             // X is 1 << x, Y is 1 << y
template<typename T>
class table {
  public:
    table(int xbits, int ybits)
        : _xbits(xbits),
          _xsize(static_cast<size_t>(1) << xbits),
          _ysize(static_cast<size_t>(1) << ybits), _i(1) {
        _v = (T**) ::calloc(_ysize, sizeof(T*));
        _v[0] = (T*) ::calloc(_xsize, sizeof(T));
    }

    ~table() {
        for (size_t i = 0; i < _i; ++i) ::free(_v[i]);
        ::free(_v);
    }

    table(const table&) = delete;
    void operator=(const table&) = delete;

    T& operator[](size_t i) {
        const size_t q = i >> _xbits;      // i / _xsize
        const size_t r = i & (_xsize - 1); // i % _xsize
        assert(q < _ysize);

        if (!_v[q]) {
            std::lock_guard<std::mutex> g(_mtx);
            if (!_v[q]) {
                _v[q] = (T*) ::calloc(_xsize, sizeof(T));
                if (_i <= q) _i = q + 1;
            }
        }
        return _v[q][r];
    }

  private:
    std::mutex _mtx;
    const size_t _xbits;
    const size_t _xsize;
    const size_t _ysize;
    size_t _i;
    T** _v;
};

} // co
