#pragma once

#include "def.h"
#include "fastring.h"

namespace co {

// return a random number (thread-safe)
__coapi int rand();

// return a random string with default symbols (thread-safe)
//   - @n: length of the random string (>0), 15 by default
__coapi fastring randstr(int n = 15);

// return a random string with specific symbols (thread-safe)
//   - @s: a null-terminated string stores the symbols to be used,
//         2 <= strlen(s) <= 255, otherwise return an empty string.
//   - @n: length of the random string, n > 0
__coapi fastring randstr(const char* s, int n);

} // co
