#ifndef _WIN32
#include "hook.h"

#ifndef _CO_DISABLE_HOOK
#include "scheduler.h"
#include "co/defer.h"
#include "co/table.h"
#include <stdarg.h>
#include <errno.h>
#include <dlfcn.h>
#include <vector>

DEF_bool(hook_log, false, "#1 enable log for hook if true");
DEF_bool(disable_hook_sleep, false, "#1 disable hook sleep if true");

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
    Hook() : _tb(14, 17) {}
    ~Hook() = default;

    HookCtx& get_hook_ctx(int s) {
        return _tb[s];
    }

  private:
    Table<HookCtx> _tb;
};

} // co

inline co::Hook& gHook() {
    static co::Hook hook;
    return hook;
}

inline struct hostent* gHostEnt() {
    static std::vector<struct hostent> ents(co::scheduler_num());
    return &ents[co::gSched->id()];
}

inline co::Mutex& gDnsMutex_t() {
    static std::vector<co::Mutex> mtx(co::scheduler_num());
    return mtx[co::gSched->id()];
}

inline co::Mutex& gDnsMutex_g() {
    static co::Mutex mtx;
    return mtx;
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


#define init_hook(f) \
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
    init_hook(socket);
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
    init_hook(socketpair);
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
    init_hook(pipe);
    int r = CO_RAW_API(pipe)(pipefd);
    if (r == 0) {
        gHook().get_hook_ctx(pipefd[0]).set_sock_or_pipe();
        gHook().get_hook_ctx(pipefd[1]).set_sock_or_pipe();
        HOOKLOG << "hook pipe, fd: " << pipefd[0] << ", " << pipefd[1];
    }
    return r;
}

int pipe2(int pipefd[2], int flags) {
    init_hook(pipe2);
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

int fcntl(int fd, int cmd, ... /* arg */) {
    init_hook(fcntl);
    if (fd < 0) { errno = EBADF; return -1; }

    va_list args;
    va_start(args, cmd);

    int r, v;
    auto& ctx = gHook().get_hook_ctx(fd);
    if (!ctx.is_sock_or_pipe()) {
        r = CO_RAW_API(fcntl)(fd, cmd, args); 
        va_end(args);
        return r;
    }

    if (cmd == F_SETFL) {
        v = va_arg(args, int);
        va_end(args);
        r = CO_RAW_API(fcntl)(fd, cmd, v); 
        if (r != -1) {
            ctx.set_non_blocking(v & O_NONBLOCK);
            HOOKLOG << "hook fcntl F_SETFL, fd: " << fd << ", non_block: " << (v & O_NONBLOCK);
        }

    } else if (cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC) {
        v = va_arg(args, int);
        va_end(args);
        r = CO_RAW_API(fcntl)(fd, cmd, v); 
        if (r != -1) {
            gHook().get_hook_ctx(r) = ctx;
            HOOKLOG << "hook fcntl F_DUPFD, fd: " << fd << ", r: " << r;
        }

    } else {
        r = CO_RAW_API(fcntl)(fd, cmd, args); 
        va_end(args);
    }

    HOOKLOG << "hook fcntl cmd: " << cmd << ", fd: " << fd << ", r: " << r;
    return r;
}

int ioctl(int fd, co::ioctl_param<ioctl_fp_t>::type request, ...) {
    init_hook(ioctl);
    if (fd < 0) { errno = EBADF; return -1; }

    va_list args;
    va_start(args, request);
    void* arg = va_arg(args, void*);
    va_end(args);

    auto& ctx = gHook().get_hook_ctx(fd);
    int r = CO_RAW_API(ioctl)(fd, request, arg);
    if (r != -1) {
        if (request == FIONBIO && ctx.is_sock_or_pipe()) {
            ctx.set_non_blocking(*(int*)arg);
            HOOKLOG << "hook ioctl FIONBIO, fd: " << fd << ", non_block: " << *(int*)arg; 
        }
    }

    HOOKLOG << "hook ioctl, fd: " << fd << ", req: " << request << ", r: " << r;
    return r;
}

int dup(int oldfd) {
    init_hook(dup);

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
    init_hook(dup2);
    if (oldfd < 0 || newfd < 0 || oldfd == newfd) return CO_RAW_API(dup2)(oldfd, newfd);

    int r = CO_RAW_API(dup2)(oldfd, newfd);
    if (r != -1) {
        gHook().get_hook_ctx(newfd) = gHook().get_hook_ctx(oldfd);
    }

    HOOKLOG << "hook dup2, oldfd: " << oldfd << ", newfd: " << newfd << ", r: " << r;
    return r;
}

int dup3(int oldfd, int newfd, int flags) {
    init_hook(dup3);
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
    init_hook(setsockopt);

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
    init_hook(close);
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
    init_hook(shutdown);
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
    init_hook(connect);
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
    init_hook(accept);
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
    init_hook(read);
    HOOKLOG << "hook read, fd: " << fd << ", n: " << count;
    if (!co::gSched || fd < 0) return CO_RAW_API(read)(fd, buf, count);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (!ctx.is_sock_or_pipe() || ctx.is_non_blocking()) return CO_RAW_API(read)(fd, buf, count);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_read);
    do_hook(CO_RAW_API(read)(fd, buf, count), ev, ctx.recv_timeout());
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
    init_hook(readv);
    HOOKLOG << "hook readv, fd: " << fd << ", n: " << iovcnt;
    if (!co::gSched || fd < 0) return CO_RAW_API(readv)(fd, iov, iovcnt);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (!ctx.is_sock_or_pipe() || ctx.is_non_blocking()) return CO_RAW_API(readv)(fd, iov, iovcnt);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_read);
    do_hook(CO_RAW_API(readv)(fd, iov, iovcnt), ev, ctx.recv_timeout());
}

ssize_t recv(int fd, void* buf, size_t len, int flags) {
    init_hook(recv);
    HOOKLOG << "hook recv, fd: " << fd << ", n: " << len;
    if (!co::gSched || fd < 0) return CO_RAW_API(recv)(fd, buf, len, flags);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(recv)(fd, buf, len, flags);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_read);
    do_hook(CO_RAW_API(recv)(fd, buf, len, flags), ev, ctx.recv_timeout());
}

ssize_t recvfrom(int fd, void* buf, size_t len, int flags, struct sockaddr* addr, socklen_t* addrlen) {
    init_hook(recvfrom);
    HOOKLOG << "hook recvfrom, fd: " << fd << ", n: " << len;
    if (!co::gSched || fd < 0) return CO_RAW_API(recvfrom)(fd, buf, len, flags, addr, addrlen);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(recvfrom)(fd, buf, len, flags, addr, addrlen);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_read);
    do_hook(CO_RAW_API(recvfrom)(fd, buf, len, flags, addr, addrlen), ev, ctx.recv_timeout());
}

ssize_t recvmsg(int fd, struct msghdr* msg, int flags) {
    init_hook(recvmsg);
    HOOKLOG << "hook recvmsg, fd: " << fd;
    if (!co::gSched || fd < 0) return CO_RAW_API(recvmsg)(fd, msg, flags);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(recvmsg)(fd, msg, flags);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_read);
    do_hook(CO_RAW_API(recvmsg)(fd, msg, flags), ev, ctx.recv_timeout());
}

ssize_t write(int fd, const void* buf, size_t count) {
    init_hook(write);
    HOOKLOG << "hook write, fd: " << fd << ", n: " << count;
    if (!co::gSched || fd < 0) return CO_RAW_API(write)(fd, buf, count);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (!ctx.is_sock_or_pipe() || ctx.is_non_blocking()) return CO_RAW_API(write)(fd, buf, count);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_write);
    do_hook(CO_RAW_API(write)(fd, buf, count), ev, ctx.send_timeout());
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    init_hook(writev);
    HOOKLOG << "hook writev, fd: " << fd << ", n: " << iovcnt;
    if (!co::gSched || fd < 0) return CO_RAW_API(writev)(fd, iov, iovcnt);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (!ctx.is_sock_or_pipe() || ctx.is_non_blocking()) return CO_RAW_API(writev)(fd, iov, iovcnt);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_write);
    do_hook(CO_RAW_API(writev)(fd, iov, iovcnt), ev, ctx.send_timeout());
}

ssize_t send(int fd, const void* buf, size_t len, int flags) {
    init_hook(send);
    HOOKLOG << "hook send, fd: " << fd << ", n: " << len;
    if (!co::gSched || fd < 0) return CO_RAW_API(send)(fd, buf, len, flags);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(send)(fd, buf, len, flags);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_write);
    do_hook(CO_RAW_API(send)(fd, buf, len, flags), ev, ctx.send_timeout());
}

ssize_t sendto(int fd, const void* buf, size_t len, int flags, const struct sockaddr* addr, socklen_t addrlen) {
    init_hook(sendto);
    HOOKLOG << "hook sendto, fd: " << fd << ", n: " << len;
    if (!co::gSched || fd < 0) return CO_RAW_API(sendto)(fd, buf, len, flags, addr, addrlen);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(sendto)(fd, buf, len, flags, addr, addrlen);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_write);
    do_hook(CO_RAW_API(sendto)(fd, buf, len, flags, addr, addrlen), ev, ctx.send_timeout());
}

ssize_t sendmsg(int fd, const struct msghdr* msg, int flags) {
    init_hook(sendmsg);
    HOOKLOG << "hook sendmsg, fd: " << fd;
    if (!co::gSched || fd < 0) return CO_RAW_API(sendmsg)(fd, msg, flags);

    auto& ctx = gHook().get_hook_ctx(fd);
    if (ctx.is_non_blocking()) return CO_RAW_API(sendmsg)(fd, msg, flags);

    if (!ctx.has_nb_mark()) { set_non_blocking(fd, 1); ctx.set_nb_mark(); }
    co::IoEvent ev(fd, co::ev_write);
    do_hook(CO_RAW_API(sendmsg)(fd, msg, flags), ev, ctx.send_timeout());
}

int poll(struct pollfd* fds, nfds_t nfds, int ms) {
    init_hook(poll);
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
    init_hook(select);
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
    do {
        int x = CO_RAW_API(select)(nfds, r, w, e, &o);
        if (x != 0 || ms == 0) return x;
        if ((uint32)ms < t) t = (uint32)ms;
        co::gSched->sleep(t);
        if (ms > 0) ms -= (int)t;
    } while (true);
}

unsigned int sleep(unsigned int n) {
    init_hook(sleep);
    HOOKLOG << "hook sleep, sec: " << n;
    if (!co::gSched || FLG_disable_hook_sleep) return CO_RAW_API(sleep)(n);
    co::gSched->sleep(n * 1000);
    return 0;
}

int usleep(useconds_t us) {
    init_hook(usleep);
    HOOKLOG << "hook usleep, us: " << us;
    if (!co::gSched || FLG_disable_hook_sleep) return CO_RAW_API(usleep)(us);

    uint32 ms = (us == 0 ? 0 : (us <= 1000 ? 1 : us / 1000));
    co::gSched->sleep(ms);
    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
    init_hook(nanosleep);
    if (!co::gSched || FLG_disable_hook_sleep) return CO_RAW_API(nanosleep)(req, rem);
    if (!req || req->tv_sec < 0 || req->tv_nsec < 0) return CO_RAW_API(nanosleep)(req, rem);

    uint32 ms = (uint32) (req->tv_sec * 1000 + req->tv_nsec / 1000000);
    if (ms == 0 && req->tv_nsec > 0) ms = 1;

    HOOKLOG << "hook nanosleep, ms: " << ms;
    co::gSched->sleep(ms);
    return 0;
}

#ifdef __linux__
int epoll_wait(int epfd, struct epoll_event* events, int n, int ms) {
    init_hook(epoll_wait);
    HOOKLOG << "hook epoll_wait, fd: " << epfd << ", n: " << n << ", ms: " << ms;
    if (!co::gSched || epfd < 0 || ms == 0) return CO_RAW_API(epoll_wait)(epfd, events, n, ms);

    co::IoEvent ev(epfd, co::ev_read);
    if (!ev.wait(ms)) return 0; // timeout
    return CO_RAW_API(epoll_wait)(epfd, events, n, 0);
}

int accept4(int fd, struct sockaddr* addr, socklen_t* addrlen, int flags) {
    init_hook(accept4);
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
    init_hook(gethostbyname_r);
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
    init_hook(gethostbyname2_r);
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
    init_hook(gethostbyaddr_r);
    HOOKLOG << "hook gethostbyaddr_r";
    if (!co::gSched) return CO_RAW_API(gethostbyaddr_r)(addr, addrlen, type, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex_t());
    return CO_RAW_API(gethostbyaddr_r)(addr, addrlen, type, ret, buf, len, res, err);
}

#ifdef NETDB_INTERNAL
struct hostent* gethostbyname2(const char* name, int af) {
    init_hook(gethostbyname2);
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
    init_hook(kevent);
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
    init_hook(gethostbyname);
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
    init_hook(gethostbyaddr);
    HOOKLOG << "hook gethostbyaddr";
    if (!co::gSched) return CO_RAW_API(gethostbyaddr)(addr, len, type);

    co::MutexGuard g(gDnsMutex_g());
    struct hostent* r = CO_RAW_API(gethostbyaddr)(addr, len, type);
    if (!r) return 0;

    struct hostent* ent = gHostEnt();
    *ent = *r;
    return ent;
}

static bool init_hooks();
static bool _dummy = init_hooks();

static bool init_hooks() {
    (void) _dummy;
    //Timer t;
    //defer(LOG << "init hooks in " << t.us() << " us");
    init_hook(socket);
    init_hook(socketpair);
    init_hook(pipe);
    init_hook(pipe2);
    init_hook(fcntl);
    init_hook(ioctl);
    init_hook(dup);
    init_hook(dup2);
    init_hook(dup3);
    init_hook(setsockopt);

    init_hook(close);
    init_hook(shutdown);
    init_hook(connect);
    init_hook(accept);
    init_hook(read);
    init_hook(readv);
    init_hook(recv);
    init_hook(recvfrom);
    init_hook(recvmsg);
    init_hook(write);
    init_hook(writev);
    init_hook(send);
    init_hook(sendto);
    init_hook(sendmsg);
    init_hook(poll);
    init_hook(select);
    init_hook(sleep);
    init_hook(usleep);
    init_hook(nanosleep);
    init_hook(gethostbyname);
    init_hook(gethostbyaddr);

  #ifdef __linux__
    init_hook(epoll_wait);
    init_hook(accept4);
    init_hook(gethostbyname2);
    init_hook(gethostbyname_r);
    init_hook(gethostbyname2_r);
    init_hook(gethostbyaddr_r);
  #else
    init_hook(kevent);
  #endif

    (void) gHook();
    return true;
}

} // "C"

#undef do_hook
#undef init_hook

#endif
#endif
