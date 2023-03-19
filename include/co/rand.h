#pragma once

#include "def.h"
#include "fastring.h"

namespace co {

// generate a random number (thread-safe)
__coapi uint32 rand();

// generate a random number with specific seed
// - @seed: 0 < seed < 2^31-1, initialize it with co::rand() for simplicity
inline uint32 rand(uint32& seed) {
    static const uint32 M = 2147483647u;  // 2^31-1
    static const uint64 A = 16385;        // 2^14+1
    const uint64 p = seed * A;
    seed = static_cast<uint32>((p >> 31) + (p & M));
    return seed > M ? (seed -= M) : seed;
}

// return a random string with default symbols (thread-safe)
// - @n: length of the random string (>0), 15 by default
__coapi fastring randstr(int n = 15);

// return a random string with specific symbols (thread-safe)
// - @s: a null-terminated string stores the symbols to be used,
//       2 <= strlen(s) <= 255, otherwise return an empty string.
// - @n: length of the random string, n > 0
__coapi fastring randstr(const char* s, int n);

} // co
