#pragma once

#ifndef _WIN32
#include <errno.h>
#endif

namespace co {

#ifdef _WIN32
int error();
void error(int e);
#else
inline int error() { return errno; }
inline void error(int e) { errno = e; }
#endif

// get string of a error number (thread-safe)
const char* strerror(int e);

// get string of the current error number (thread-safe)
inline const char* strerror() {
    return co::strerror(co::error());
}

} // co
