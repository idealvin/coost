#ifndef _WIN32
#include "hook.h"

#ifdef _CO_DISABLE_HOOK
namespace co {

void init_hook() {}
void cleanup_hook() {}
void disable_hook_sleep() {}
void enable_hook_sleep() {}

} // co

#else
#include "scheduler.h"
#include "co/alloc.h"
#include "co/defer.h"
#include "co/stl/table.h"
#include <stdarg.h>
#include <errno.h>
#include <dlfcn.h>
#include <vector>

DEF_bool(hook_log, false, ">>#1 enable log for hook");

#define HOOKLOG DLOG_IF(FLG_hook_log)

namespace co {

class HookCtx {
  public:
    HookCtx() : _v(0) {}
    HookCtx(const HookCtx& s) { _v = s._v; }
    ~HookCtx() = default;

    // set blocking or non_blocking mode for the socket.
    void set_non_blocking(int v) {
        _s.nb = !!v;
        if (_s.nb_mark) _s.nb_mark = 0;
    }

    bool is_non_blocking() const { return _s.nb; }
    bool is_blocking() const { return !this->is_non_blocking(); }

    // for blocking and non-overlapped sockets, we modify them to non_blocking, 
    // and set nb_mark to 1.
    void set_nb_mark() { _s.nb_mark = 1; }
    bool has_nb_mark() const { return _s.nb_mark; }

    // if the timeout is greater than 65535 ms, we truncate it to 65535.
    void set_send_timeout(uint32 ms) { _s.send_timeout = ms <= 65535 ? (uint16)ms : 65535; }
    void set_recv_timeout(uint32 ms) { _s.recv_timeout = ms <= 65535 ? (uint16)ms : 65535; }

    int send_timeout() const { return _s.send_timeout == 0 ? -1 : _s.send_timeout; }
    int recv_timeout() const { return _s.recv_timeout == 0 ? -1 : _s.recv_timeout; }

    void clear() { _v = 0; }

    static const uint8 f_shut_read = 1;
    static const uint8 f_shut_write = 2;

    void set_shut_read() {
        if (atomic_or(&_s.flags, f_shut_read) & f_shut_write) this->clear();
    }

    void set_shut_write() {
        if (atomic_or(&_s.flags, f_shut_write) & f_shut_read) this->clear();
    }

    void set_sock_or_pipe() { _s.so = 1; }
    bool is_sock_or_pipe() const { return _s.so; }

  private:
    union {
        uint64 _v;
        struct {
            uint8  nb;      // non_blocking
            uint8  so;      // socket or pipe fd
            uint8  nb_mark; // non_blocking mark
            uint8  flags;
            uint16 recv_timeout;
            uint16 send_timeout;
        } _s;
    };
};

class Hook {
  public:
    Hook() : tb(14, 17), hook_sleep(true) {}
    ~Hook() = default;

    HookCtx& get_hook_ctx(int s) {
        return tb[s];
    }

    bool hook_sleep;
    co::table<HookCtx> tb;
};

} // co

inline co::Hook& gHook() {
    static auto hook = co::new_static<co::Hook>();
    return *hook;
}

inline struct hostent* gHostEnt() {
    static auto ents = co::new_static<std::vector<struct hostent>>(co::scheduler_num());
    return &(*ents)[co::gSched->id()];
}

inline co::Mutex& gDnsMutex_t() {
    static auto mtxs = co::new_static<std::vector<co::Mutex>>(co::scheduler_num());
    return (*mtxs)[co::gSched->id()];
}

inline co::Mutex& gDnsMutex_g() {
    static auto mtx = co::new_static<co::Mutex>();
    return *mtx;
}


extern "C" {

_CO_DEF_RAW_API(socket);
_CO_DEF_RAW_API(socketpair);
_CO_DEF_RAW_API(pipe);
_CO_DEF_RAW_API(pipe2);
_CO_DEF_RAW_API(fcntl);
_CO_DEF_RAW_API(ioctl);
_CO_DEF_RAW_API(dup);
_CO_DEF_RAW_API(dup2);
_CO_DEF_RAW_API(dup3);
_CO_DEF_RAW_API(setsockopt);
_CO_DEF_RAW_API(close);
_CO_DEF_RAW_API(shutdown);
_CO_DEF_RAW_API(connect);
_CO_DEF_RAW_API(accept);
_CO_DEF_RAW_API(read);
_CO_DEF_RAW_API(readv);
_CO_DEF_RAW_API(recv);
_CO_DEF_RAW_API(recvfrom);
_CO_DEF_RAW_API(recvmsg);
_CO_DEF_RAW_API(write);
_CO_DEF_RAW_API(writev);
_CO_DEF_RAW_API(send);
_CO_DEF_RAW_API(sendto);
_CO_DEF_RAW_API(sendmsg);
_CO_DEF_RAW_API(poll);
_CO_DEF_RAW_API(select);
_CO_DEF_RAW_API(sleep);
_CO_DEF_RAW_API(usleep);
_CO_DEF_RAW_API(nanosleep);
_CO_DEF_RAW_API(gethostbyname);
_CO_DEF_RAW_API(gethostbyaddr);

#ifdef __linux__
_CO_DEF_RAW_API(epoll_wait);
_CO_DEF_RAW_API(accept4);
_CO_DEF_RAW_API(gethostbyname2);
_CO_DEF_RAW_API(gethostbyname_r);
_CO_DEF_RAW_API(gethostbyname2_r);
_CO_DEF_RAW_API(gethostbyaddr_r);
#else
_CO_DEF_RAW_API(kevent);
#endif


#define hook_api(f) \
    if (!CO_RAW_API(f)) atomic_set(&CO_RAW_API(f), dlsym(RTLD_NEXT, #f))

#define do_hook(f, ev, ms) \
    do { \
        auto r = f; \
        if (r != -1) return r; \
        if (errno == EWOULDBLOCK || errno == EAGAIN) { \
            if (!ev.wait(ms)) return -1; \
        } else if (errno != EINTR) { \
            return -1; \
        } \
    } while (true)


inline void set_non_blocking(int fd, int x) {
    CO_RAW_API(ioctl)(fd, FIONBIO, (char*)&x);
}

int socket(int domain, int type, int protocol) {
    hook_api(socket);
    int s = CO_RAW_API(socket)(domain, type, protocol);
    if (s != -1) {
        auto& ctx = gHook().get_hook_ctx(s);
        ctx.set_sock_or_pipe();
      #ifdef SOCK_NONBLOCK
        if (type & SOCK_NONBLOCK) ctx.set_non_blocking(1);
      #endif
        HOOKLOG << "hook socket, sock: " << s << ", non_block: " << ctx.is_non_blocking();
    }
    return s;
}

int socketpair(int domain, int type, int protocol, int sv[2]) {
    hook_api(socketpair);
    int r = CO_RAW_API(socketpair)(domain, type, protocol, sv);
    if (r == 0) {
        auto& ctx0 = gHook().get_hook_ctx(sv[0]);
        auto& ctx1 = gHook().get_hook_ctx(sv[1]);
        ctx0.set_sock_or_pipe();
        ctx1.set_sock_or_pipe();
      #ifdef SOCK_NONBLOCK
        if (type & SOCK_NONBLOCK) {
            ctx0.set_non_blocking(1);
            ctx1.set_non_blocking(1);
        }
      #endif
        HOOKLOG << "hook socketpair, sock: " << sv[0] << ", " << sv[1] << ", non_block: " << ctx0.is_non_blocking();
    }
    return r;
}

int pipe(int pipefd[2]) {
    hook_api(pipe);
    int r = CO_RAW_API(pipe)(pipefd);
    if (r == 0) {
        gHook().get_hook_ctx(pipefd[0]).set_sock_or_pipe();
        gHook().get_hook_ctx(pipefd[1]).set_sock_or_pipe();
        HOOKLOG << "hook pipe, fd: " << pipefd[0] << ", " << pipefd[1];
    }
    return r;
}

int pipe2(int pipefd[2], int flags) {
    hook_api(pipe2);
    int r = CO_RAW_API(pipe2)(pipefd, flags);
    if (r == 0) {
        auto& ctx0 = gHook().get_hook_ctx(pipefd[0]);
        auto& ctx1 = gHook().get_hook_ctx(pipefd[1]);
        ctx0.set_sock_or_pipe();
        ctx1.set_sock_or_pipe();
        if (flags & O_NONBLOCK) {
            ctx0.set_non_blocking(1);
            ctx1.set_non_blocking(1);
        }
        HOOKLOG << "hook pipe2, fd: " << pipefd[0] << ", " << pipefd[1] << ", non_block: " << (flags & O_NONBLOCK);
    }
    return r;
}

#ifndef F_DUPFD_CLOEXEC
#define F_DUPFD_CLOEXEC F_DUPFD 
#endif

/**
 * A dirty hack to get the variadic arguments of fcntl, ioctl, etc.
 *   - It works when the storage of the variadic arguments is <= 8*sizeof(intptr_t),
 *     which should be enough for most cases.
 */
#define VARG_8_GET(args) \
    intptr_t a1,a2,a3,a4,a5,a6,a7,a8; { \
        a1=va_arg(args,intptr_t); a2=va_arg(args,intptr_t); \
        a3=va_arg(args,intptr_t); a4=va_arg(args,intptr_t); \
        a5=va_arg(args,intptr_t); a6=va_arg(args,intptr_t); \
        a7=va_arg(args,intptr_t); a8=va_arg(args,intptr_t); \
    }

#define VARG_8_PARAMS a1,a2,a3,a4,a5,a6,a7,a8


int fcntl(int fd, int cmd, ... /* arg */) {
    hook_api(fcntl);
    if (fd < 0) { errno = EBADF; return -1; }

    va_list args;
    va_start(args, cmd);

    int r, v;
    auto& ctx = gHook().get_hook_ctx(fd);

    if (cmd == F_SETFL || cmd == F_GETFL || cmd == F_SETFD || cmd == F_GETFD || 
        cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC) {
        v = va_arg(args, int);
        va_end(args);
        r = CO_RAW_API(fcntl)(fd, cmd, v);
        if (r != -1 && ctx.is_sock_or_pipe()) {
            if (cmd == F_SETFL) {
                ctx.set_non_blocking(v & O_NONBLOCK);
                HOOKLOG << "hook fcntl F_SETFL, fd: " << fd << ", non_block: " << (v & O_NONBLOCK);
            } else if (cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC) {
                gHook().get_hook_ctx(r) = ctx;
                HOOKLOG << "hook fcntl F_DUPFD, fd: " << fd << ", r: " << r;
            }
        }

    } else {
        VARG_8_GET(args);
        va_end(args);
        r = CO_RAW_API(fcntl)(fd, cmd, VARG_8_PARAMS);
    }

    HOOKLOG << "hook fcntl cmd: " << cmd << ", fd: " << fd << ", r: " << r;
    return r;
}

int ioctl(int fd, co::ioctl_param<ioctl_fp_t>::type request, ...) {
    hook_api(ioctl);
    if (fd < 0) { errno = EBADF; return -1; }

    va_list args;
    va_start(args, request);

    int r;
    auto& ctx = gHook().get_hook_ctx(fd);
    
    if (request == FIONBIO) {
        void* arg = va_arg(args, void*);
        int v = *(int*)arg;
        va_end(args);
        r = CO_RAW_API(ioctl)(fd, request, arg);
        if (r != -1 && ctx.is_sock_or_pipe()) {
            ctx.set_non_blocking(v);
            HOOKLOG << "hook ioctl FIONBIO, fd: " << fd << ", non_block: " << v; 
        }

    } else {
        VARG_8_GET(args);
        va_end(args);
        r = CO_RAW_API(ioctl)(fd, request, VARG_8_PARAMS);
    }

    HOOKLOG << "hook ioctl, fd: " << fd << ", req: " << request << ", r: " << r;
    return r;
}

int dup(int oldfd) {
    hook_api(dup);

    int r = CO_RAW_API(dup)(oldfd);
    if (r != -1) {
        auto& ctx = gHook().get_hook_ctx(oldfd);
        if (ctx.is_sock_or_pipe()) {
            gHook().get_hook_ctx(r) = ctx;
        }
    }

    HOOKLOG << "hook dup, oldfd: " << oldfd << ", r: " << r;
    return r;
}

int dup2(int oldfd, int newfd) {
    hook_api(dup2);
    if (oldfd < 0 || newfd < 0 || oldfd == newfd) return CO_RAW_API(dup2)(oldfd, newfd);

    int r = CO_RAW_API(dup2)(oldfd, newfd);
    if (r != -1) {
        gHook().get_hook_ctx(newfd) = gHook().get_hook_ctx(oldfd);
    }

    HOOKLOG << "hook dup2, oldfd: " << oldfd << ", newfd: " << newfd << ", r: " << r;
    return r;
}

int dup3(int oldfd, int newfd, int flags) {
    hook_api(dup3);
    if (!CO_RAW_API(dup3)) { errno = ENOSYS; return -1; }
    if (oldfd < 0 || newfd < 0 || oldfd == newfd) return CO_RAW_API(dup3)(oldfd, newfd, flags);

    int r = CO_RAW_API(dup3)(oldfd, newfd, flags);
    if (r != -1) {
        gHook().get_hook_ctx(newfd) = gHook().get_hook_ctx(oldfd);
    }

    HOOKLOG << "hook dup3, oldfd: " << oldfd << ", newfd: " << newfd << ", flags: " << flags << ", r: " << r;
    return r;
}

int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
    hook_api(setsockopt);

    int r = CO_RAW_API(setsockopt)(sockfd, level, optname, optval, optlen);
    if (r == 0 && level == SOL_SOCKET && (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO)) {
        auto tv = (const struct timeval*) optval;
        const int64 us = (int64)tv->tv_sec * 1000000 + tv->tv_usec;
        int ms = (us == 0 ? 0 : (us >= 1000 ? (int)(us / 1000) : 1));
        if (optname == SO_RCVTIMEO) {
            HOOKLOG << "hook_setsockopt, sock: " << sockfd << ", recv timeout: " << ms;
            gHook().get_hook_ctx(sockfd).set_recv_timeout(ms);
        } else {
            HOOKLOG << "hook_setsockopt, sock: " << sockfd << ", send timeout: " << ms;
            gHook().get_hook_ctx(sockfd).set_send_timeout(ms);
        }
    }
    return r;
}

int close(int fd) {
    hook_api(close);
    HOOKLOG << "hook close, fd: " << fd;
    if (fd < 0) return CO_RAW_API(close)(fd);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_sock_or_pipe()) {
        ctx.clear();
        return co::close(fd);
    } else {
        return CO_RAW_API(close)(fd);
    }
}

int shutdown(int fd, int how) {
    hook_api(shutdown);
    HOOKLOG << "hook shutdown, fd: " << fd << ", how: " << how;
    if (fd < 0) return CO_RAW_API(shutdown)(fd, how);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (how == SHUT_RD) {
        ctx.set_shut_read();
        return co::shutdown(fd, 'r');
    } else if (how == SHUT_WR) {
        ctx.set_shut_write();
        return co::shutdown(fd, 'w');
    } else {
        ctx.clear();
        return co::shutdown(fd, 'b');
    }
}

/*
 * From man-pages socket(7):  (man 7 socket)
 * SO_RCVTIMEO and SO_SNDTIMEO
 *     Specify the receiving or sending timeouts until reporting an error. The
 *     argument is a struct timeval. If an input or output function blocks for
 *     this period of time, and data has been sent or received, the return value
 *     of that function will be the amount of data transferred; if no data has
 *     been transferred and the timeout has been reached, then -1 is returned
 *     with errno set to EAGAIN or EWOULDBLOCK, or EINPROGRESS (for connect(2))
 *     just as if the socket was specified to be nonblocking.
 */
int connect(int fd, const struct sockaddr* addr, socklen_t addrlen) {
    hook_api(connect);
    HOOKLOG << "hook connect, fd: " << fd;
    if (!co::gSched || fd < 0) return CO_RAW_API(connect)(fd, addr, addrlen);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(connect)(fd, addr, addrlen);

    set_non_blocking(fd, 1);
    defer(set_non_blocking(fd, 0));

    int r = co::connect(fd, addr, addrlen, ctx.send_timeout());
    if (r == -1 && errno == ETIMEDOUT) errno = EINPROGRESS; // set errno to EINPROGRESS
    return r;
}

int accept(int fd, struct sockaddr* addr, socklen_t* addrlen) {
    hook_api(accept);
    HOOKLOG << "hook accept, fd: " << fd;
    if (!co::gSched || fd < 0) return CO_RAW_API(accept)(fd, addr, addrlen);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(accept)(fd, addr, addrlen);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }

    int conn_fd;
    co::IoEvent ev(fd, co::ev_read);
    defer(HOOKLOG << "hook accept, fd: " << fd << ", conn_fd: " << conn_fd);
    do {
        conn_fd = CO_RAW_API(accept)(fd, addr, addrlen);
        if (conn_fd != -1) return conn_fd;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            ev.wait();
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

ssize_t read(int fd, void* buf, size_t count) {
    hook_api(read);
    HOOKLOG << "hook read, fd: " << fd << ", n: " << count;
    if (!co::gSched || fd < 0) return CO_RAW_API(read)(fd, buf, count);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (!ctx.is_sock_or_pipe() || ctx.is_non_blocking()) return CO_RAW_API(read)(fd, buf, count);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_read);
    do_hook(CO_RAW_API(read)(fd, buf, count), ev, ctx.recv_timeout());
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
    hook_api(readv);
    HOOKLOG << "hook readv, fd: " << fd << ", n: " << iovcnt;
    if (!co::gSched || fd < 0) return CO_RAW_API(readv)(fd, iov, iovcnt);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (!ctx.is_sock_or_pipe() || ctx.is_non_blocking()) return CO_RAW_API(readv)(fd, iov, iovcnt);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_read);
    do_hook(CO_RAW_API(readv)(fd, iov, iovcnt), ev, ctx.recv_timeout());
}

ssize_t recv(int fd, void* buf, size_t len, int flags) {
    hook_api(recv);
    HOOKLOG << "hook recv, fd: " << fd << ", n: " << len;
    if (!co::gSched || fd < 0) return CO_RAW_API(recv)(fd, buf, len, flags);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(recv)(fd, buf, len, flags);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_read);
    do_hook(CO_RAW_API(recv)(fd, buf, len, flags), ev, ctx.recv_timeout());
}

ssize_t recvfrom(int fd, void* buf, size_t len, int flags, struct sockaddr* addr, socklen_t* addrlen) {
    hook_api(recvfrom);
    HOOKLOG << "hook recvfrom, fd: " << fd << ", n: " << len;
    if (!co::gSched || fd < 0) return CO_RAW_API(recvfrom)(fd, buf, len, flags, addr, addrlen);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(recvfrom)(fd, buf, len, flags, addr, addrlen);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_read);
    do_hook(CO_RAW_API(recvfrom)(fd, buf, len, flags, addr, addrlen), ev, ctx.recv_timeout());
}

ssize_t recvmsg(int fd, struct msghdr* msg, int flags) {
    hook_api(recvmsg);
    HOOKLOG << "hook recvmsg, fd: " << fd;
    if (!co::gSched || fd < 0) return CO_RAW_API(recvmsg)(fd, msg, flags);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(recvmsg)(fd, msg, flags);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_read);
    do_hook(CO_RAW_API(recvmsg)(fd, msg, flags), ev, ctx.recv_timeout());
}

ssize_t write(int fd, const void* buf, size_t count) {
    hook_api(write);
    HOOKLOG << "hook write, fd: " << fd << ", n: " << count;
    if (!co::gSched || fd < 0) return CO_RAW_API(write)(fd, buf, count);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (!ctx.is_sock_or_pipe() || ctx.is_non_blocking()) return CO_RAW_API(write)(fd, buf, count);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_write);
    do_hook(CO_RAW_API(write)(fd, buf, count), ev, ctx.send_timeout());
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    hook_api(writev);
    HOOKLOG << "hook writev, fd: " << fd << ", n: " << iovcnt;
    if (!co::gSched || fd < 0) return CO_RAW_API(writev)(fd, iov, iovcnt);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (!ctx.is_sock_or_pipe() || ctx.is_non_blocking()) return CO_RAW_API(writev)(fd, iov, iovcnt);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_write);
    do_hook(CO_RAW_API(writev)(fd, iov, iovcnt), ev, ctx.send_timeout());
}

ssize_t send(int fd, const void* buf, size_t len, int flags) {
    hook_api(send);
    HOOKLOG << "hook send, fd: " << fd << ", n: " << len;
    if (!co::gSched || fd < 0) return CO_RAW_API(send)(fd, buf, len, flags);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(send)(fd, buf, len, flags);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_write);
    do_hook(CO_RAW_API(send)(fd, buf, len, flags), ev, ctx.send_timeout());
}

ssize_t sendto(int fd, const void* buf, size_t len, int flags, const struct sockaddr* addr, socklen_t addrlen) {
    hook_api(sendto);
    HOOKLOG << "hook sendto, fd: " << fd << ", n: " << len;
    if (!co::gSched || fd < 0) return CO_RAW_API(sendto)(fd, buf, len, flags, addr, addrlen);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(sendto)(fd, buf, len, flags, addr, addrlen);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_write);
    do_hook(CO_RAW_API(sendto)(fd, buf, len, flags, addr, addrlen), ev, ctx.send_timeout());
}

ssize_t sendmsg(int fd, const struct msghdr* msg, int flags) {
    hook_api(sendmsg);
    HOOKLOG << "hook sendmsg, fd: " << fd;
    if (!co::gSched || fd < 0) return CO_RAW_API(sendmsg)(fd, msg, flags);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(sendmsg)(fd, msg, flags);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_write);
    do_hook(CO_RAW_API(sendmsg)(fd, msg, flags), ev, ctx.send_timeout());
}

int poll(struct pollfd* fds, nfds_t nfds, int ms) {
    hook_api(poll);
    HOOKLOG << "hook poll, nfds: " << nfds << ", ms: " << ms;
    if (!co::gSched || ms == 0) return CO_RAW_API(poll)(fds, nfds, ms);

    do {
        if (nfds == 1) {
            int fd = fds[0].fd;
            if (fd < 0) break;
            auto& ctx = gHook().get_hook_ctx(fd);
            if (!ctx.is_sock_or_pipe() || !ctx.is_non_blocking()) break;

            if (fds[0].events == POLLIN) {
                if (!co::gSched->add_io_event(fd, co::ev_read)) break;
            } else if (fds[0].events == POLLOUT) {
                if (!co::gSched->add_io_event(fd, co::ev_write)) break;
            } else {
                break;
            }

            if (ms > 0) co::gSched->add_timer(ms);
            co::gSched->yield();
            co::gSched->del_io_event(fd);
            if (ms > 0 && co::gSched->timeout()) return 0;

            fds[0].revents = fds[0].events;
            return 1;
        }
    } while (0);

    // it's boring to hook poll when nfds > 1, just check poll every 16 ms
    uint32 t = 16;
    do {
        int r = CO_RAW_API(poll)(fds, nfds, 0);
        if (r != 0 || ms == 0) return r;
        if ((uint32)ms < t) t = (uint32)ms;
        co::sleep(t);
        if (ms > 0) ms -= (int)t;
    } while (true);
}

int select(int nfds, fd_set* r, fd_set* w, fd_set* e, struct timeval* tv) {
    hook_api(select);
    if (!co::gSched) return CO_RAW_API(select)(nfds, r, w, e, tv);

    int ms = -1;
    if (tv) {
        const int64 us = (int64)tv->tv_sec * 1000000 + tv->tv_usec;
        ms = (us == 0 ? 0 : (us >= 1000 ? (int)(us / 1000) : 1));
    }

    HOOKLOG << "hook select, nfds: " << nfds << ", ms: " << ms;
    if ((nfds == 0 || (!r && !w && !e)) && ms > 0) {
        co::gSched->sleep(ms);
        return 0;
    }

    // it's boring to hook select, just check select every 16 ms
    uint32 t = 16;
    struct timeval o = { 0, 0 };
    fd_set s[3];
    if (r) s[0] = *r;
    if (w) s[1] = *w;
    if (e) s[2] = *e;

    do {
        int x = CO_RAW_API(select)(nfds, r, w, e, &o);
        if (x != 0 || ms == 0) return x;
        if ((uint32)ms < t) t = (uint32)ms;
        co::gSched->sleep(t);
        if (ms > 0) ms -= (int)t;
        if (r) *r = s[0];
        if (w) *w = s[1];
        if (e) *e = s[2];
    } while (true);
}

unsigned int sleep(unsigned int n) {
    hook_api(sleep);
    HOOKLOG << "hook sleep, sec: " << n;
    if (!co::gSched || !gHook().hook_sleep) return CO_RAW_API(sleep)(n);
    co::gSched->sleep(n * 1000);
    return 0;
}

int usleep(useconds_t us) {
    hook_api(usleep);
    HOOKLOG << "hook usleep, us: " << us;
    if (!co::gSched || !gHook().hook_sleep) return CO_RAW_API(usleep)(us);

    uint32 ms = (us == 0 ? 0 : (us <= 1000 ? 1 : us / 1000));
    co::gSched->sleep(ms);
    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
    hook_api(nanosleep);
    if (!co::gSched || !gHook().hook_sleep) return CO_RAW_API(nanosleep)(req, rem);
    if (!req || req->tv_sec < 0 || req->tv_nsec < 0) return CO_RAW_API(nanosleep)(req, rem);

    uint32 ms = (uint32) (req->tv_sec * 1000 + req->tv_nsec / 1000000);
    if (ms == 0 && req->tv_nsec > 0) ms = 1;

    HOOKLOG << "hook nanosleep, ms: " << ms;
    co::gSched->sleep(ms);
    return 0;
}

#ifdef __linux__
int epoll_wait(int epfd, struct epoll_event* events, int n, int ms) {
    hook_api(epoll_wait);
    HOOKLOG << "hook epoll_wait, fd: " << epfd << ", n: " << n << ", ms: " << ms;
    if (!co::gSched || epfd < 0 || ms == 0) return CO_RAW_API(epoll_wait)(epfd, events, n, ms);

    co::IoEvent ev(epfd, co::ev_read);
    if (!ev.wait(ms)) return 0; // timeout
    return CO_RAW_API(epoll_wait)(epfd, events, n, 0);
}

int accept4(int fd, struct sockaddr* addr, socklen_t* addrlen, int flags) {
    hook_api(accept4);
    if (!CO_RAW_API(accept4)) { errno = ENOSYS; return -1; }

    HOOKLOG << "hook accept4, fd: " << fd;
    if (!co::gSched || fd < 0) return CO_RAW_API(accept4)(fd, addr, addrlen, flags);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(accept4)(fd, addr, addrlen, flags);

    int conn_fd;
    co::IoEvent ev(fd, co::ev_read);
    defer(HOOKLOG << "hook accept4, fd: " << fd << ", conn_fd: " << conn_fd);
    do {
        conn_fd = CO_RAW_API(accept4)(fd, addr, addrlen, flags);
        if (conn_fd != -1) return conn_fd;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            ev.wait();
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int gethostbyname_r(
    const char* name,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    hook_api(gethostbyname_r);
    HOOKLOG << "hook gethostbyname_r, name: " << (name ? name : "");
    if (!co::gSched) return CO_RAW_API(gethostbyname_r)(name, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex_t());
    return CO_RAW_API(gethostbyname_r)(name, ret, buf, len, res, err);
}

int gethostbyname2_r(
    const char* name, int af,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    hook_api(gethostbyname2_r);
    HOOKLOG << "hook gethostbyname2_r, name: " << (name ? name : "");
    if (!co::gSched) return CO_RAW_API(gethostbyname2_r)(name, af, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex_t());
    return CO_RAW_API(gethostbyname2_r)(name, af, ret, buf, len, res, err);
}

int gethostbyaddr_r(
    const void* addr, socklen_t addrlen, int type,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    hook_api(gethostbyaddr_r);
    HOOKLOG << "hook gethostbyaddr_r";
    if (!co::gSched) return CO_RAW_API(gethostbyaddr_r)(addr, addrlen, type, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex_t());
    return CO_RAW_API(gethostbyaddr_r)(addr, addrlen, type, ret, buf, len, res, err);
}

#ifdef NETDB_INTERNAL
struct hostent* gethostbyname2(const char* name, int af) {
    hook_api(gethostbyname2);
    HOOKLOG << "hook gethostbyname2, name: " << (name ? name : "");
    if (!co::gSched || !name) return CO_RAW_API(gethostbyname2)(name, af);

    fastream fs(1024);
    struct hostent* ent = gHostEnt();
    struct hostent* res = 0;
    int* err = (int*) fs.data();

    int r = -1;
    while (true) {
        r = gethostbyname2_r(name, af, ent, (char*)(fs.data() + 8), fs.capacity() - 8, &res, err);
        if (r == ERANGE && *err == NETDB_INTERNAL) {
            fs.reserve(fs.capacity() << 1);
            err = (int*) fs.data();
        } else {
            break;
        }
    }

    if (r == 0 && ent == res) return res;
    return 0;
}
#endif

#else
int kevent(int kq, const struct kevent* c, int nc, struct kevent* e, int ne, const struct timespec* ts) {
    hook_api(kevent);
    HOOKLOG << "hook kevent, kq: " << kq << ", nc: " << nc;
    if (!co::gSched || c || kq < 0) return CO_RAW_API(kevent)(kq, c, nc, e, ne, ts);

    int ms = -1;
    if (ts) ms = ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
    if (ms == 0 && ts->tv_nsec > 0) ms = 1;
    if (ms == 0) return CO_RAW_API(kevent)(kq, c, nc, e, ne, ts);

    co::IoEvent ev(kq, co::ev_read);
    if (!ev.wait(ms)) return 0; // timeout
    return CO_RAW_API(kevent)(kq, c, nc, e, ne, 0);
}
#endif

struct hostent* gethostbyname(const char* name) {
    hook_api(gethostbyname);
    HOOKLOG << "hook gethostbyname, name: " << (name ? name : "");
    if (!co::gSched) return CO_RAW_API(gethostbyname)(name);

    co::MutexGuard g(gDnsMutex_g());
    struct hostent* r = CO_RAW_API(gethostbyname)(name);
    if (!r) return 0;

    struct hostent* ent = gHostEnt();
    *ent = *r;
    return ent;
}

struct hostent* gethostbyaddr(const void* addr, socklen_t len, int type) {
    hook_api(gethostbyaddr);
    HOOKLOG << "hook gethostbyaddr";
    if (!co::gSched) return CO_RAW_API(gethostbyaddr)(addr, len, type);

    co::MutexGuard g(gDnsMutex_g());
    struct hostent* r = CO_RAW_API(gethostbyaddr)(addr, len, type);
    if (!r) return 0;

    struct hostent* ent = gHostEnt();
    *ent = *r;
    return ent;
}

} // "C"

namespace co {

// DO NOT call _init_hooks() elsewhere.
static bool _init_hooks();
static bool _dummy = _init_hooks();

static bool _init_hooks() {
    (void) _dummy;
    hook_api(socket);
    hook_api(socketpair);
    hook_api(pipe);
    hook_api(pipe2);
    hook_api(fcntl);
    hook_api(ioctl);
    hook_api(dup);
    hook_api(dup2);
    hook_api(dup3);
    hook_api(setsockopt);

    hook_api(close);
    hook_api(shutdown);
    hook_api(connect);
    hook_api(accept);
    hook_api(read);
    hook_api(readv);
    hook_api(recv);
    hook_api(recvfrom);
    hook_api(recvmsg);
    hook_api(write);
    hook_api(writev);
    hook_api(send);
    hook_api(sendto);
    hook_api(sendmsg);
    hook_api(poll);
    hook_api(select);
    hook_api(sleep);
    hook_api(usleep);
    hook_api(nanosleep);
    hook_api(gethostbyname);
    hook_api(gethostbyaddr);

  #ifdef __linux__
    hook_api(epoll_wait);
    hook_api(accept4);
    hook_api(gethostbyname2);
    hook_api(gethostbyname_r);
    hook_api(gethostbyname2_r);
    hook_api(gethostbyaddr_r);
  #else
    hook_api(kevent);
  #endif

    return true;
}

void init_hook() {
    if (CO_RAW_API(close) == 0) { (void) ::close(-1); }
    if (CO_RAW_API(read) == 0)  { auto r = ::read(-1, 0, 0);  (void)r; }
    if (CO_RAW_API(write) == 0) { auto r = ::write(-1, 0, 0); (void)r; }
    if (CO_RAW_API(pipe) == 0)  { auto r = ::pipe((int*)0);   (void)r; }
    if (CO_RAW_API(fcntl) == 0) { (void) ::fcntl(-1, 0); }
    CHECK(CO_RAW_API(close) != 0);
    CHECK(CO_RAW_API(read) != 0);
    CHECK(CO_RAW_API(write) != 0);
    CHECK(CO_RAW_API(pipe) != 0);
    CHECK(CO_RAW_API(fcntl) != 0);

  #ifdef __linux__
    if (CO_RAW_API(epoll_wait) == 0) ::epoll_wait(-1, 0, 0, 0);
    CHECK(CO_RAW_API(epoll_wait) != 0);
  #else
    if (CO_RAW_API(kevent) == 0) ::kevent(-1, 0, 0, 0, 0, 0);
    CHECK(CO_RAW_API(kevent) != 0);
  #endif
}

void cleanup_hook() {
    atomic_swap(&gHook().hook_sleep, false);
}

void disable_hook_sleep() {
    atomic_swap(&gHook().hook_sleep, false);
}

void enable_hook_sleep() {
    atomic_swap(&gHook().hook_sleep, true);
}

} // co

#undef do_hook
#undef init_hook
#undef HOOKLOG

#endif
#endif
