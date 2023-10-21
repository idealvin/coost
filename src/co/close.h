#ifndef _WIN32
#include "hook.h"
#include <unistd.h>

#if defined(__linux__)
#include <sys/syscall.h>

#ifndef SYS_close
#define SYS_close __NR_close
#endif

inline int _close_nocancel(int fd) {
    return syscall(SYS_close, fd);
}

#elif defined(__APPLE__)

int _close_nocancel(int fd);

#elif defined(_hpux) || defined(__hpux)
#include <errno.h>

inline int _close_nocancel(int fd) {
    int r;
    while ((r = __sys_api(close)(fd)) != 0 && errno == EINTR);
    return r;
}

#else
inline int _close_nocancel(int fd) {
    return __sys_api(close)(fd);
}
#endif
#endif // #ifndef _WIN32
