#pragma once

#include "def.h"
#include <assert.h>
#include <string.h>
#include <vector>

class MemPool {
  public:
    MemPool(uint32 block_size, uint32 cap=32)
        : _block_size(block_size), _cap(cap), _idx(0) {
        _p = (char*) malloc(_block_size * cap);
        _avail.reserve(cap);
    }

    ~MemPool() {
        free(_p);
    }

    uint32 alloc() {
        if (_idx < _cap) return _idx++;

        if (!_avail.empty()) {
            uint32 idx = _avail.back();
            _avail.pop_back();
            return idx;
        }

        _cap += ((_cap >> 1) + 1);
        _p = (char*) realloc(_p, _block_size * _cap);
        assert(_p);
        return _idx++;
    }

    void dealloc(uint32 idx) {
        _avail.push_back(idx);
        if (_avail.size() == (size_t)_idx) {
            _avail.clear();
            _idx = 0;
        }
    }

    void* at(uint32 idx) {
        return _p + _block_size * idx;
    }

  private:
    uint32 _block_size;
    uint32 _cap;
    uint32 _idx;
    char* _p;
    std::vector<uint32> _avail;

    DISALLOW_COPY_AND_ASSIGN(MemPool);
};
