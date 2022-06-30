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

#elif defined(__APPLE__) && !TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
#include <dlfcn.h>

inline int _close_nocancel(int fd) {
    typedef int (*close_t)(int);
    static close_t f = []() {
        void* p = dlsym(RTLD_DEFAULT, "close$NOCANCEL");
        if (!p) p = dlsym(RTLD_DEFAULT, "close$NOCANCEL$UNIX2003");
        return (close_t)p;
    }();
    return f ? f(fd) : CO_RAW_API(close)(fd);
}

#elif defined(_hpux) || defined(__hpux)
#include <errno.h>

inline int _close_nocancel(int fd) {
    int r;
    while ((r = CO_RAW_API(close)(fd)) != 0 && errno == EINTR);
    return r;
}

#else
inline int _close_nocancel(int fd) {
    return CO_RAW_API(close)(fd);
}
#endif
#endif // #ifndef _WIN32
