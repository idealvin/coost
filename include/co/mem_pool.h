#pragma once

#include "def.h"
#include <assert.h>
#include <stdlib.h>
#include <vector>

// B: block size
// P: page size, default is 4096
template <int B, int P=4096>
class MemPool {
  public:
    MemPool() {
        static_assert(!(B & (B - 1)) && P > 0 && P % B == 0, "...");
        _current_page = (char*) malloc(P);
        _current_index = 0;
        _pages.reserve(256);
        _pages.push_back(_current_page);
        _blocks.reserve(P / B);
    }

    ~MemPool() {
        for (size_t i = 0; i < _pages.size(); ++i) {
            free(_pages[i]);
        }
    }

    MemPool(MemPool&& p)
        : _pages(std::move(p._pages)), _blocks(std::move(p._blocks)),
          _current_page(p._current_page), _current_index(p._current_index) {
    }

    void* alloc() {
        if (_current_index < (P / B)) {
            return _current_page + (_current_index++) * B;
        }

        if (!_blocks.empty()) {
            void* block = _blocks.back();
            _blocks.pop_back();
            return block;
        }

        _current_index = 1;
        _current_page = (char*) malloc(P);
        _pages.push_back(_current_page);
        return _current_page;
    }

    void dealloc(void* p) {
        _blocks.push_back(p);
    }

  private:
    std::vector<char*> _pages;
    std::vector<void*> _blocks;
    char* _current_page;
    size_t _current_index;

    DISALLOW_COPY_AND_ASSIGN(MemPool);
};
