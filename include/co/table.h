#pragma once

#include <assert.h>
#include <stdlib.h>
#include <mutex>

namespace co {

/**
 * A fixed-size table.
 *   - It stores elements in a 2-dimensional array.
 *   - Memory of the elements are zero-cleared.
 * 
 *   - usage:
 *     co::table<int> t(12, 5); // 128k(4k x 32) elements can be stored in the table
 *     int v = t[88];           // v is 0 as the internal memory is zero-cleared
 *     t[32] = 77;
 */
template <typename T>
class table {
  public:
    /**
     * A x * y table.
     *   - x is 1 << xbits, y is 1 << ybits.
     *   - There are x elements in a row, and y rows in total.
     */
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
