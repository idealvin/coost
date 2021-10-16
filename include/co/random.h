#pragma once

#include "def.h"

class __codec Random {
  public:
    Random() : Random(1u) {}

    explicit Random(uint32 seed) : _seed(seed & 0x7fffffffu) {
        if (_seed == 0 || _seed == 2147483647L) _seed = 1;
    }

    uint32 next() {
        static const uint32 M = 2147483647L;  // 2^31-1
        static const uint64 A = 16385;        // 2^14+1

        // Computing _seed * A % M.
        const uint64 p = _seed * A;
        _seed = static_cast<uint32>((p >> 31) + (p & M));
        if (_seed > M) _seed -= M;

        return _seed;
    }

  private:
    uint32 _seed;
};
