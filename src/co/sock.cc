#ifndef _WIN32
#include "close.h"
#include "sched.h"

#ifdef __APPLE__
#include <dlfcn.h>

typedef int (*close_t)(int);
static close_t g_close_nocancel;
std::once_flag g_close_flag;

int _close_nocancel(int fd) {
    std::call_once(g_close_flag, []() {
        void* p = dlsym(RTLD_DEFAULT, "close$NOCANCEL");
        if (!p) p = dlsym(RTLD_DEFAULT, "close$NOCANCEL$UNIX2003");
        g_close_nocancel = (close_t)p;
    });
    return g_close_nocancel ? g_close_nocancel(fd) : __sys_api(close)(fd);
}
#endif

namespace co {

void set_nonblock(sock_t fd) {
    __sys_api(fcntl)(fd, F_SETFL, __sys_api(fcntl)(fd, F_GETFL) | O_NONBLOCK);
}

void set_cloexec(sock_t fd) {
    __sys_api(fcntl)(fd, F_SETFD, __sys_api(fcntl)(fd, F_GETFD) | FD_CLOEXEC);
}

#ifdef SOCK_NONBLOCK
sock_t socket(int domain, int type, int protocol) {
    return __sys_api(socket)(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
}

#else
sock_t socket(int domain, int type, int protocol) {
    sock_t fd = __sys_api(socket)(domain, type, protocol);
    if (fd != -1) {
        co::set_nonblock(fd);
        co::set_cloexec(fd);
    }
    return fd;
}
#endif

int close(sock_t fd, int ms) {
    if (fd < 0) return 0;
    const auto sched = xx::gSched;
    if (sched) {
        sched->del_io_event(fd);
        if (ms > 0) sched->sleep(ms);
    } else {
        co::get_sock_ctx(fd).del_event();
    }
    return _close_nocancel(fd);
}

int shutdown(sock_t fd, char c) {
    if (fd < 0) return 0;
    const auto sched = xx::gSched;
    int how;
    if (sched) {
        switch (c) {
          case 'r':
            sched->del_io_event(fd, ev_read);
            how = SHUT_RD;
            break;
          case 'w':
            sched->del_io_event(fd, ev_write);
            how = SHUT_WR;
            break;
          default:
            sched->del_io_event(fd);
            how = SHUT_RDWR;
        } 

    } else {
        switch (c) {
          case 'r':
            co::get_sock_ctx(fd).del_ev_read();
            how = SHUT_RD;
            break;
          case 'w':
            co::get_sock_ctx(fd).del_ev_write();
            how = SHUT_WR;
            break;
          default:
            co::get_sock_ctx(fd).del_event();
            how = SHUT_RDWR;
        } 
    }

    return __sys_api(shutdown)(fd, how);
}

int bind(sock_t fd, const void* addr, int addrlen) {
    return ::bind(fd, (const struct sockaddr*)addr, (socklen_t)addrlen);
}

int listen(sock_t fd, int backlog) {
    return ::listen(fd, backlog);
}

sock_t accept(sock_t fd, void* addr, int* addrlen) {
    const auto sched = xx::gSched;
    CHECK(sched) << "must be called in coroutine..";

    io_event ev(fd, ev_read);
    do {
      #ifdef SOCK_NONBLOCK
        sock_t connfd = __sys_api(accept4)(fd, (sockaddr*)addr, (socklen_t*)addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (connfd != -1) return connfd;
      #else
        sock_t connfd = __sys_api(accept)(fd, (sockaddr*)addr, (socklen_t*)addrlen);
        if (connfd != -1) {
            co::set_nonblock(connfd);
            co::set_cloexec(connfd);
            return connfd;
        }
      #endif

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            ev.wait();
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int connect(sock_t fd, const void* addr, int addrlen, int ms) {
    const auto sched = xx::gSched;
    CHECK(sched) << "must be called in coroutine..";

    do {
        int r = __sys_api(connect)(fd, (const sockaddr*)addr, (socklen_t)addrlen);
        if (r == 0) return 0;

        if (errno == EINPROGRESS) {
            io_event ev(fd, ev_write);
            if (!ev.wait(ms)) return -1;

            int err, len = sizeof(err);
            r = co::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
            if (r != 0) return -1;
            if (err == 0) return 0;
            errno = err;
            return -1;

        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int recv(sock_t fd, void* buf, int n, int ms) {
    const auto sched = xx::gSched;
    CHECK(sched) << "must be called in coroutine..";

    io_event ev(fd, ev_read);
    do {
        int r = (int) __sys_api(recv)(fd, buf, n, 0);
        if (r != -1) return r;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            if (!ev.wait(ms)) return -1;
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int recvn(sock_t fd, void* buf, int n, int ms) {
    char* p = (char*) buf;
    int remain = n;
    io_event ev(fd, ev_read);
    do {
        int r = (int) __sys_api(recv)(fd, p, remain, 0);
        if (r == remain) return n;
        if (r == 0) return 0;

        if (r == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                if (!ev.wait(ms)) return -1;
            } else if (errno != EINTR) {
                return -1;
            }
        } else {
            remain -= r;
            p += r;
        }
    } while (true);
}

int recvfrom(sock_t fd, void* buf, int n, void* addr, int* addrlen, int ms) {
    const auto sched = xx::gSched;
    CHECK(sched) << "must be called in coroutine..";

    io_event ev(fd, ev_read);
    do {
        int r = (int) __sys_api(recvfrom)(fd, buf, n, 0, (sockaddr*)addr, (socklen_t*)addrlen);
        if (r != -1) return r;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            if (!ev.wait(ms)) return -1;
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int send(sock_t fd, const void* buf, int n, int ms) {
    const auto sched = xx::gSched;
    CHECK(sched) << "must be called in coroutine..";

    const char* p = (const char*) buf;
    int remain = n;
    io_event ev(fd, ev_write);

    do {
        int r = (int) __sys_api(send)(fd, p, remain, 0);
        if (r == remain) return n;

        if (r == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                if (!ev.wait(ms)) return -1;
            } else if (errno != EINTR) {
                return -1;
            }
        } else {
            remain -= r;
            p += r;
        }
    } while (true);
}

int sendto(sock_t fd, const void* buf, int n, const void* addr, int addrlen, int ms) {
    const auto sched = xx::gSched;
    CHECK(sched) << "must be called in coroutine..";

    const char* p = (const char*) buf;
    int remain = n;
    io_event ev(fd, ev_write);

    do {
        int r = (int) __sys_api(sendto)(fd, p, remain, 0, (const sockaddr*)addr, (socklen_t)addrlen);
        if (r == remain) return n;

        if (r == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                if (!ev.wait(ms)) return -1;
            } else if (errno != EINTR) {
                return -1;
            }
        } else {
            remain -= r;
            p += r;
        }
    } while (true);
}

} // co

#endif
