#pragma once

class Random {
  public:
    explicit Random(unsigned int seed = 1) : _seed(seed & 0x7fffffffu) {
        if (_seed == 0 || _seed == 2147483647L) _seed = 1;
    }

    unsigned int next() {
        static const unsigned int M = 2147483647L;  // 2^31-1
        static const unsigned long long A = 16385;  // 2^14+1

        // Computing _seed * A % M.
        unsigned long long p = _seed * A;
        _seed = static_cast<unsigned int>((p >> 31) + (p & M));
        if (_seed > M) _seed -= M;

        return _seed;
    }

  private:
    unsigned int _seed;
};
