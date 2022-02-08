#pragma once

#include <assert.h>
#include <stdlib.h>
#include <mutex>

namespace co {

/**
 * A thread-safe fixed-size table. 
 *   - It stores elements in buckets, and each bucket has a same size.
 *   - Note that, no matter what the type of T is, Table will zero-clear the memory.
 */
template<typename T>
class Table {
  public:
    /**
     * @param bucket_bitnum   bitnum of buckets, bucket_num will be (1 << bucket_bitnum).
     * @param bucket_bitsize  bitsize of each bucket, bucket_size will be (1 << bucket_bitsize).
     */
    Table(int bucket_bitnum, int bucket_bitsize)
        : _bucket_num ((size_t)(1ULL << bucket_bitnum)),
          _bucket_size((size_t)(1ULL << bucket_bitsize)),
          _bucket_bitsize(bucket_bitsize), _i(1) {
        _v = (T**) calloc(_bucket_num, sizeof(T*));
        _v[0] = (T*) calloc(_bucket_size, sizeof(T));
    }

    ~Table() {
        for (size_t i = 0; i < _i; ++i) ::free(_v[i]);
        ::free(_v);
    }

    Table(const Table&) = delete;
    void operator=(const Table&) = delete;

    T& operator[](size_t i) {
        if (i < _bucket_size) return _v[0][i];

        const size_t q = i >> _bucket_bitsize;    // i / _bucket_size
        const size_t r = i & (_bucket_size - 1);  // i % _bucket_size
        assert(q < _bucket_num);

        if (!_v[q]) {
            std::lock_guard<std::mutex> g(_mtx);
            if (!_v[q]) {
                _v[q] = (T*) calloc(_bucket_size, sizeof(T));
                if (_i <= q) _i = q + 1;
            }
        }
        return _v[q][r];
    }

  private:
    std::mutex _mtx;
    const size_t _bucket_num;
    const size_t _bucket_size;
    const size_t _bucket_bitsize;
    size_t _i;
    T** _v;
};

} // co
