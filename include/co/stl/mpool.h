#pragma once

#include "vector.h"

namespace co {

/**
 * memory pool
 * 
 *   @param T     object type
 *   @param Bits  bit size of a block
 */
template <typename T, int Bits = 17>
class mpool {
  public:
    static const size_t _blk_size = (size_t)(1ULL << Bits);
    static const size_t _max_index = _blk_size / sizeof(T);

    mpool() : _p(0), _e(0) {}
    mpool(size_t bcap, size_t vcap) : _b(bcap), _v(vcap), _p(0), _e(0) {}

    ~mpool() {
        for (auto& x: _b) ::free(x);
    }

    T* pop() {
        return !_v.empty() ? _v.pop_back() : (_p < _e ? _p++ : this->_alloc_from_new_block());
    }

    void push(T* p) {
        _v.push_back(p);
    }

    void clear() {
        _v.clear();
        for (auto& x: _b) ::free(x);
        _b.clear();
        _p = _e = 0;
    }

    void reset() {
        _v.reset();
        for (auto& x: _b) ::free(x);
        _b.reset();
        _p = _e = 0;
    }

  private:
    T* _alloc_from_new_block() {
        _p = (T*) ::malloc(_blk_size); assert(_p);
        _e = _p + _max_index;
        _b.push_back(_p);
        return _p++;
    }

  private:
    co::vector<T*> _b; // blocks
    co::vector<T*> _v;    
    T* _p; // position of the current block
    T* _e; // end of the current block
};

} // co
