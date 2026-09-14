#pragma once

#ifndef _WIN32
#include <unistd.h>

#if defined(_hpux) || defined(__hpux)
#include <errno.h>

// see https://www.man7.org/linux/man-pages/man2/close.2.html
inline int _close(int fd) {
    int r;
    while ((r = ::close(fd)) != 0 && errno == EINTR);
    return r;
}

#else
inline int _close(int fd) {
    return ::close(fd);
}

#endif // if defined(_hpux) || defined(__hpux)
#endif // ifndef _WIN32
