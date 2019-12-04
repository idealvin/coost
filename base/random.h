#pragma once

#include <stdint.h>

class Random {
  public:
    Random() : Random(1u) {}

    explicit Random(uint32_t seed) : _seed(seed & 0x7fffffffu) {
        if (_seed == 0 || _seed == 2147483647L) _seed = 1;
    }

    uint32_t next() {
        static const uint32_t M = 2147483647L;  // 2^31-1
        static const uint64_t A = 16385;  // 2^14+1

        // Computing _seed * A % M.
        uint64_t p = _seed * A;
        _seed = static_cast<uint32_t>((p >> 31) + (p & M));
        if (_seed > M) _seed -= M;

        return _seed;
    }

  private:
    uint32_t _seed;
};
