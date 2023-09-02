#pragma once

#include "def.h"
#include "fastring.h"

namespace co {

// generate a random number (0 < result < 2^31-1, thread-safe)
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

// return a random string with default symbols ("_-0-9a-zA-Z", thread-safe)
// - @n: length of the random string, 15 by default
__coapi fastring randstr(int n = 15);

// return a random string with specific symbols (thread-safe)
// - @s: a null-terminated string stores the symbols (length < 256),
//       abbreviation like "0-9", "a-f" can be used.
// - @n: length of the random string
__coapi fastring randstr(const char* s, int n);

} // co
