#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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
