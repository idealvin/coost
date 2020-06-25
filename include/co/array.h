#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4200)
#endif

#include "def.h"
#include <assert.h>
#include <stdlib.h>

class Array {
  public:
    typedef const void* T;

    Array() : Array(32) {}

    explicit Array(uint32 cap) {
        _mem = (_Mem*) malloc(sizeof(_Mem) + sizeof(T) * cap);
        _mem->cap = cap;
        _mem->size = 0;
    }

    ~Array() {
        free(_mem);
    }

    uint32 capicity() const {
        return _mem->cap;
    }

    uint32 size() const {
        return _mem->size;
    }

    bool empty() const {
        return this->size() == 0;
    }

    T& front() const {
        return _mem->p[0];
    }

    T& back() const {
        return _mem->p[this->size() - 1];
    }

    T& operator[](uint32 i) const {
        return _mem->p[i];
    }

    void push_back(T v) {
        if (_mem->size == _mem->cap) {
            _mem = (_Mem*) realloc(_mem, sizeof(_Mem) + sizeof(T) * (_mem->cap << 1));
            assert(_mem);
            _mem->cap <<= 1;
        }
        _mem->p[_mem->size++] = v;
    }

    T pop_back() {
        return _mem->p[--_mem->size];
    }

    struct _Mem {
        uint32 cap;
        uint32 size;
        T p[];
    }; // 8 bytes

    _Mem* _mem;
};
