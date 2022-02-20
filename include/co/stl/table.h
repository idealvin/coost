#pragma once

#include <assert.h>
#include <stdlib.h>
#include <mutex>

namespace co {

/**
 * A fixed-size table. 
 *   - It stores elements in a 2-dimensional array.
 *   - Memory of the elements are zero-cleared.
 */
template <typename T>
class table {
  public:
    /**
     * @param xbits  bit size for x-dimension, the size is (1 << xbits).
     * @param ybits  bit size for y-dimension, the size is (1 << ybits).
     */
    table(int xbits, int ybits)
        : _xsize((size_t)(1ULL << xbits)),
          _ysize((size_t)(1ULL << ybits)),
          _ybits(ybits), _i(1) {
        _v = (T**) calloc(_xsize, sizeof(T*));
        _v[0] = (T*) calloc(_ysize, sizeof(T));
    }

    ~table() {
        for (size_t i = 0; i < _i; ++i) ::free(_v[i]);
        ::free(_v);
    }

    table(const table&) = delete;
    void operator=(const table&) = delete;

    T& operator[](size_t i) {
        const size_t q = i >> _ybits;      // i / _ysize
        const size_t r = i & (_ysize - 1); // i % _ysize
        assert(q < _xsize);

        if (!_v[q]) {
            std::lock_guard<std::mutex> g(_mtx);
            if (!_v[q]) {
                _v[q] = (T*) calloc(_ysize, sizeof(T));
                if (_i <= q) _i = q + 1;
            }
        }
        return _v[q][r];
    }

  private:
    std::mutex _mtx;
    const size_t _xsize;
    const size_t _ysize;
    const size_t _ybits;
    size_t _i;
    T** _v;
};

} // co
