#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace err {

inline int get() { return GetLastError(); }

inline void set(int e) { SetLastError(e); }

} // err

#else
#include <errno.h>

namespace err {

inline int get() { return errno; }

inline void set(int e) { errno = e; }

} // err

#endif
