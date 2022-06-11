#pragma once

#include "def.h"

#ifndef _WIN32
#include <errno.h>
#endif

namespace co {

#ifdef _WIN32
__coapi int& error();
#else
inline int& error() { return errno; }
#endif

// get string of a error number (thread-safe)
__coapi const char* strerror(int e);

// get string of the current error number (thread-safe)
inline const char* strerror() {
    return co::strerror(co::error());
}

} // co
