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
#ifdef __APPLE__
#include "fishhook/fishhook.h"
#include <sys/uio.h>
#endif
#include "scheduler.h"
#include "co/cout.h"
#include "co/defer.h"
#include "co/table.h"
#include <stdarg.h>
#include <errno.h>
#include <dlfcn.h>

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

    // for blocking sockets, we modify them to non_blocking, 
    // and set nb_mark to 1.
    void set_nb_mark() { _s.nb_mark = 1; }
    bool has_nb_mark() const { return _s.nb_mark; }

    // TODO: handle timeout > 65535
    // if the timeout is greater than 65535 ms, we truncate it to 65535.
    void set_send_timeout(uint32 ms) { _s.send_timeout = ms <= 65535 ? (uint16)ms : 65535; }
    void set_recv_timeout(uint32 ms) { _s.recv_timeout = ms <= 65535 ? (uint16)ms : 65535; }

    int send_timeout() const { return _s.send_timeout == 0 ? -1 : _s.send_timeout; }
    int recv_timeout() const { return _s.recv_timeout == 0 ? -1 : _s.recv_timeout; }

    void clear() { _v = 0; }

    static const uint8 f_shut_read = 1;
    static const uint8 f_shut_write = 2;

    void set_shut_read() {
        if (atomic_or(&_s.flags, f_shut_read, mo_acq_rel) & f_shut_write) this->clear();
    }

    void set_shut_write() {
        if (atomic_or(&_s.flags, f_shut_write, mo_acq_rel) & f_shut_read) this->clear();
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

    HookCtx* get_hook_ctx(int s) {
        return s >= 0 ? &tb[s] : NULL;
    }

    co::table<HookCtx> tb;
    bool hook_sleep;
};

} // co

inline co::Hook& gHook() {
    static auto hook = co::static_new<co::Hook>();
    return *hook;
}

inline struct hostent* gHostEnt() {
    static auto ents = co::static_new<co::vector<struct hostent>>(co::scheduler_num());
    return &(*ents)[co::gSched->id()];
}

inline co::Mutex& gDnsMutex_t() {
    static auto mtxs = co::static_new<co::vector<co::Mutex>>(co::scheduler_num());
    return (*mtxs)[co::gSched->id()];
}

inline co::Mutex& gDnsMutex_g() {
    static auto mtx = co::static_new<co::Mutex>();
    return *mtx;
}


extern "C" {

_CO_DEF_SYS_API(socket);
_CO_DEF_SYS_API(socketpair);
_CO_DEF_SYS_API(pipe);
_CO_DEF_SYS_API(pipe2);
_CO_DEF_SYS_API(fcntl);
_CO_DEF_SYS_API(ioctl);
_CO_DEF_SYS_API(dup);
_CO_DEF_SYS_API(dup2);
_CO_DEF_SYS_API(dup3);
_CO_DEF_SYS_API(setsockopt);
_CO_DEF_SYS_API(close);
_CO_DEF_SYS_API(shutdown);
_CO_DEF_SYS_API(connect);
_CO_DEF_SYS_API(accept);
_CO_DEF_SYS_API(read);
_CO_DEF_SYS_API(readv);
_CO_DEF_SYS_API(recv);
_CO_DEF_SYS_API(recvfrom);
_CO_DEF_SYS_API(recvmsg);
_CO_DEF_SYS_API(write);
_CO_DEF_SYS_API(writev);
_CO_DEF_SYS_API(send);
_CO_DEF_SYS_API(sendto);
_CO_DEF_SYS_API(sendmsg);
_CO_DEF_SYS_API(poll);
_CO_DEF_SYS_API(select);
_CO_DEF_SYS_API(sleep);
_CO_DEF_SYS_API(usleep);
_CO_DEF_SYS_API(nanosleep);
_CO_DEF_SYS_API(gethostbyname);
_CO_DEF_SYS_API(gethostbyaddr);

#ifdef __linux__
_CO_DEF_SYS_API(epoll_wait);
_CO_DEF_SYS_API(accept4);
_CO_DEF_SYS_API(gethostbyname2);
_CO_DEF_SYS_API(gethostbyname_r);
_CO_DEF_SYS_API(gethostbyname2_r);
_CO_DEF_SYS_API(gethostbyaddr_r);
#else
_CO_DEF_SYS_API(kevent);
#endif

#ifdef __APPLE__
#define _hook(f) hook_##f
#define _hook_api(f)
#define hook_api(f) \
    if (!__sys_api(f)) { \
        void* origin = 0;   \
        rebind_symbols((rebinding[1]){{#f, (void*)::_hook(f), (void**)&origin}}, 1); \
        atomic_store(&__sys_api(f), origin, mo_relaxed); \
    }
#else
#define _hook(f) f
#define _hook_api(f) \
    if (!__sys_api(f)) atomic_store(&__sys_api(f), dlsym(RTLD_NEXT, #f), mo_relaxed)
#define hook_api(f) _hook_api(f)
#endif

#define do_hook(f, ev, ms) \
    do { \
        r = f; \
        if (r != -1) goto end; \
        if (errno == EWOULDBLOCK || errno == EAGAIN) { \
            if (!ev.wait(ms)) goto end; \
        } else if (errno != EINTR) { \
            goto end; \
        } \
    } while (true)


inline void set_non_blocking(int fd, int x) {
    __sys_api(ioctl)(fd, FIONBIO, (char*)&x);
}

int _hook(socket)(int domain, int type, int protocol) {
    _hook_api(socket);
    int s = __sys_api(socket)(domain, type, protocol);
    auto ctx = gHook().get_hook_ctx(s);
    if (ctx) {
        ctx->set_sock_or_pipe();
      #ifdef SOCK_NONBLOCK
        if (type & SOCK_NONBLOCK) ctx->set_non_blocking(1);
      #endif
        HOOKLOG << "hook socket, sock: " << s << ", non_block: " << ctx->is_non_blocking();
    }
    return s;
}

int _hook(socketpair)(int domain, int type, int protocol, int sv[2]) {
    _hook_api(socketpair);
    int r = __sys_api(socketpair)(domain, type, protocol, sv);
    if (r == 0) {
        auto ctx0 = gHook().get_hook_ctx(sv[0]);
        auto ctx1 = gHook().get_hook_ctx(sv[1]);
        ctx0->set_sock_or_pipe();
        ctx1->set_sock_or_pipe();
      #ifdef SOCK_NONBLOCK
        if (type & SOCK_NONBLOCK) {
            ctx0->set_non_blocking(1);
            ctx1->set_non_blocking(1);
        }
      #endif
        HOOKLOG << "hook socketpair, sock: " << sv[0] << ", " << sv[1] << ", non_block: " << ctx0->is_non_blocking();
    }
    return r;
}

int _hook(pipe)(int fds[2]) {
    _hook_api(pipe);
    int r = __sys_api(pipe)(fds);
    if (r == 0) {
        gHook().get_hook_ctx(fds[0])->set_sock_or_pipe();
        gHook().get_hook_ctx(fds[1])->set_sock_or_pipe();
        HOOKLOG << "hook pipe, fd: " << fds[0] << ", " << fds[1];
    }
    return r;
}

int _hook(pipe2)(int fds[2], int flags) {
    _hook_api(pipe2);
    int r = __sys_api(pipe2)(fds, flags);
    if (r == 0) {
        auto ctx0 = gHook().get_hook_ctx(fds[0]);
        auto ctx1 = gHook().get_hook_ctx(fds[1]);
        const int nb = !!(flags & O_NONBLOCK);
        ctx0->set_sock_or_pipe();
        ctx1->set_sock_or_pipe();
        if (nb) {
            ctx0->set_non_blocking(1);
            ctx1->set_non_blocking(1);
        }
        HOOKLOG << "hook pipe2, fd: " << fds[0] << ", " << fds[1] << ", non_block: " << nb;
    }
    return r;
}

#ifndef F_DUPFD_CLOEXEC
#define F_DUPFD_CLOEXEC F_DUPFD 
#endif

int _hook(fcntl)(int fd, int cmd, ... /* arg */) {
    _hook_api(fcntl);

    int r;
    union { int v; void* arg; };
    auto ctx = gHook().get_hook_ctx(fd);

    va_list args;
    va_start(args, cmd);
    arg = va_arg(args, void*);
    va_end(args);

    r = __sys_api(fcntl)(fd, cmd, arg);
    if (r != -1 && ctx && ctx->is_sock_or_pipe()) {
        if (cmd == F_SETFL) {
            const int nb = !!(v & O_NONBLOCK);
            ctx->set_non_blocking(nb);
            HOOKLOG << "hook fcntl F_SETFL, fd: " << fd << ", non_block: " << nb;
        } else if (cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC) {
            *gHook().get_hook_ctx(r) = *ctx;
            HOOKLOG << "hook fcntl F_DUPFD, fd: " << fd << ", r: " << r;
        }
    }

    HOOKLOG << "hook fcntl cmd: " << cmd << ", fd: " << fd << ", r: " << r;
    return r;
}

/**
 * A dirty hack to get the variadic arguments of ioctl.
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

int _hook(ioctl)(int fd, co::ioctl_param<ioctl_fp_t>::type request, ...) {
    _hook_api(ioctl);

    int r;
    auto ctx = gHook().get_hook_ctx(fd);
    va_list args;
    va_start(args, request);
   
    if (request == FIONBIO) {
        int* arg = va_arg(args, int*);
        va_end(args);

        const int nb = *arg;
        r = __sys_api(ioctl)(fd, request, arg);
        if (r != -1 && ctx && ctx->is_sock_or_pipe()) {
            ctx->set_non_blocking(nb);
            HOOKLOG << "hook ioctl FIONBIO, fd: " << fd << ", non_block: " << nb; 
        }

    } else {
        VARG_8_GET(args);
        va_end(args);
        r = __sys_api(ioctl)(fd, request, VARG_8_PARAMS);
    }

    HOOKLOG << "hook ioctl, fd: " << fd << ", req: " << request << ", r: " << r;
    return r;
}

int _hook(dup)(int oldfd) {
    _hook_api(dup);

    int r = __sys_api(dup)(oldfd);
    if (r != -1) {
        auto ctx = gHook().get_hook_ctx(oldfd);
        if (ctx->is_sock_or_pipe()) {
            *gHook().get_hook_ctx(r) = *ctx;
        }
    }

    HOOKLOG << "hook dup, oldfd: " << oldfd << ", r: " << r;
    return r;
}

// if oldfd == newfd, dup2 does nothing and returns newfd
int _hook(dup2)(int oldfd, int newfd) {
    _hook_api(dup2);

    int r = __sys_api(dup2)(oldfd, newfd);
    if (r != -1 && oldfd != newfd) {
        *gHook().get_hook_ctx(newfd) = *gHook().get_hook_ctx(oldfd);
    }

    HOOKLOG << "hook dup2, oldfd: " << oldfd << ", newfd: " << newfd << ", r: " << r;
    return r;
}

// if oldfd == newfd, dup3 failes with EINVAL
int _hook(dup3)(int oldfd, int newfd, int flags) {
    _hook_api(dup3);
    if (!__sys_api(dup3)) { errno = ENOSYS; return -1; }

    int r = __sys_api(dup3)(oldfd, newfd, flags);
    if (r != -1) {
        *gHook().get_hook_ctx(newfd) = *gHook().get_hook_ctx(oldfd);
    }

    HOOKLOG << "hook dup3, oldfd: " << oldfd << ", newfd: " << newfd << ", flags: " << flags << ", r: " << r;
    return r;
}

int _hook(setsockopt)(int fd, int level, int optname, const void* optval, socklen_t optlen) {
    _hook_api(setsockopt);

    int r = __sys_api(setsockopt)(fd, level, optname, optval, optlen);
    if (r == 0 && level == SOL_SOCKET && (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO)) {
        auto tv = (const struct timeval*) optval;
        const int64 us = (int64)tv->tv_sec * 1000000 + tv->tv_usec;
        const int ms = (us <= 0 ? 0 : (us > 1000 ? (int)(us / 1000) : 1));
        if (optname == SO_RCVTIMEO) {
            HOOKLOG << "hook setsockopt, sock: " << fd << ", recv timeout: " << ms;
            gHook().get_hook_ctx(fd)->set_recv_timeout(ms);
        } else {
            HOOKLOG << "hook setsockopt, sock: " << fd << ", send timeout: " << ms;
            gHook().get_hook_ctx(fd)->set_send_timeout(ms);
        }
    }
    return r;
}

int _hook(close)(int fd) {
    _hook_api(close);
    if (fd < 0) { errno = EBADF; return -1; }

    int r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (ctx->is_sock_or_pipe()) {
        ctx->clear();
        r = co::close(fd);
    } else {
        r = __sys_api(close)(fd);
    }

    HOOKLOG << "hook close, fd: " << fd << ", r: " << r;
    return r;
}

int _hook(shutdown)(int fd, int how) {
    _hook_api(shutdown);
    if (fd < 0) { errno = EBADF; return -1; }

    int r;
    auto ctx = gHook().get_hook_ctx(fd);
    switch (how) {
      case SHUT_RD:
        ctx->set_shut_read();
        r = co::shutdown(fd, 'r');
        break;
      case SHUT_WR:
        ctx->set_shut_write();
        r = co::shutdown(fd, 'w');
        break;
      case SHUT_RDWR:
        ctx->clear();
        r = co::shutdown(fd, 'b');
        break;
      default:
        errno = EINVAL; // how is invalid
        r = -1;
    }

    HOOKLOG << "hook shutdown, fd: " << fd << ", how: " << how << ", r: " << r;
    return r;
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
int _hook(connect)(int fd, const struct sockaddr* addr, socklen_t addrlen) {
    _hook_api(connect);

    int r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(connect)(fd, addr, addrlen);
        goto end;
    }

    set_non_blocking(fd, 1);
    r = co::connect(fd, addr, addrlen, ctx->send_timeout());
    set_non_blocking(fd, 0);
    if (r == -1 && errno == ETIMEDOUT) errno = EINPROGRESS;

  end:
    HOOKLOG << "hook connect, fd: " << fd << ", r: " << r;
    return r;
}

int _hook(accept)(int fd, struct sockaddr* addr, socklen_t* addrlen) {
    _hook_api(accept);

    int r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(accept)(fd, addr, addrlen);
        goto end;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(fd, 1); ctx->set_nb_mark(); }
    {
        co::IoEvent ev(fd, co::ev_read);
        do {
            r = __sys_api(accept)(fd, addr, addrlen);
            if (r != -1) goto end;

            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                ev.wait();
            } else if (errno != EINTR) {
                goto end;
            }
        } while (true);
    }

  end:
    if (r != -1) gHook().get_hook_ctx(r)->set_sock_or_pipe();
    HOOKLOG << "hook accept, fd: " << fd << ", r: " << r;
    return r;
}

ssize_t _hook(read)(int fd, void* buf, size_t count) {
    _hook_api(read);

    ssize_t r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || !ctx->is_sock_or_pipe() || ctx->is_non_blocking()) {
        r = __sys_api(read)(fd, buf, count);
        goto end;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(fd, 1); ctx->set_nb_mark(); }
    {
        co::IoEvent ev(fd, co::ev_read);
        do_hook(__sys_api(read)(fd, buf, count), ev, ctx->recv_timeout());
    }

  end:
    HOOKLOG << "hook read, fd: " << fd << ", n: " << count << ", r: " << r;
    return r;
}

ssize_t _hook(readv)(int fd, const struct iovec* iov, int iovcnt) {
    _hook_api(readv);

    ssize_t r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || !ctx->is_sock_or_pipe() || ctx->is_non_blocking()) {
        r = __sys_api(readv)(fd, iov, iovcnt);
        goto end;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(fd, 1); ctx->set_nb_mark(); }
    {
        co::IoEvent ev(fd, co::ev_read);
        do_hook(__sys_api(readv)(fd, iov, iovcnt), ev, ctx->recv_timeout());
    }

  end:
    HOOKLOG << "hook readv, fd: " << fd << ", n: " << iovcnt << ", r: " << r;
    return r;
}

ssize_t _hook(recv)(int fd, void* buf, size_t len, int flags) {
    _hook_api(recv);

    ssize_t r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(recv)(fd, buf, len, flags);
        goto end;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(fd, 1); ctx->set_nb_mark(); }
    {
        co::IoEvent ev(fd, co::ev_read);
        do_hook(__sys_api(recv)(fd, buf, len, flags), ev, ctx->recv_timeout());
    }

  end:
    HOOKLOG << "hook recv, fd: " << fd << ", n: " << len << ", r: " << r;
    return r;
}

ssize_t _hook(recvfrom)(int fd, void* buf, size_t len, int flags, struct sockaddr* addr, socklen_t* addrlen) {
    _hook_api(recvfrom);

    ssize_t r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(recvfrom)(fd, buf, len, flags, addr, addrlen);
        goto end;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(fd, 1); ctx->set_nb_mark(); }
    {
        co::IoEvent ev(fd, co::ev_read);
        do_hook(__sys_api(recvfrom)(fd, buf, len, flags, addr, addrlen), ev, ctx->recv_timeout());
    }

  end:
    HOOKLOG << "hook recvfrom, fd: " << fd << ", n: " << len << ", r: " << r;
    return r;
}

ssize_t _hook(recvmsg)(int fd, struct msghdr* msg, int flags) {
    _hook_api(recvmsg);

    ssize_t r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(recvmsg)(fd, msg, flags);
        goto end;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(fd, 1); ctx->set_nb_mark(); }
    {
        co::IoEvent ev(fd, co::ev_read);
        do_hook(__sys_api(recvmsg)(fd, msg, flags), ev, ctx->recv_timeout());
    }

  end:
    HOOKLOG << "hook recvmsg, fd: " << fd << ", r: " << r;
    return r;
}

ssize_t _hook(write)(int fd, const void* buf, size_t count) {
    _hook_api(write);

    ssize_t r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || !ctx->is_sock_or_pipe() || ctx->is_non_blocking()) {
        r = __sys_api(write)(fd, buf, count);
        goto end;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(fd, 1); ctx->set_nb_mark(); }
    {
        co::IoEvent ev(fd, co::ev_write);
        do_hook(__sys_api(write)(fd, buf, count), ev, ctx->send_timeout());
    }

  end:
    HOOKLOG << "hook write, fd: " << fd << ", n: " << count << ", r: " << r;
    return r;
}

ssize_t _hook(writev)(int fd, const struct iovec* iov, int iovcnt) {
    _hook_api(writev);

    ssize_t r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || !ctx->is_sock_or_pipe() || ctx->is_non_blocking()) {
        r = __sys_api(writev)(fd, iov, iovcnt);
        goto end;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(fd, 1); ctx->set_nb_mark(); }
    {
        co::IoEvent ev(fd, co::ev_write);
        do_hook(__sys_api(writev)(fd, iov, iovcnt), ev, ctx->send_timeout());
    }

  end:
    HOOKLOG << "hook writev, fd: " << fd << ", n: " << iovcnt << ", r: " << r;
    return r;
}

ssize_t _hook(send)(int fd, const void* buf, size_t len, int flags) {
    _hook_api(send);

    ssize_t r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(send)(fd, buf, len, flags);
        goto end;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(fd, 1); ctx->set_nb_mark(); }
    {
        co::IoEvent ev(fd, co::ev_write);
        do_hook(__sys_api(send)(fd, buf, len, flags), ev, ctx->send_timeout());
    }

  end:
    HOOKLOG << "hook send, fd: " << fd << ", n: " << len << ", r: " << r;
    return r;
}

ssize_t _hook(sendto)(int fd, const void* buf, size_t len, int flags, const struct sockaddr* addr, socklen_t addrlen) {
    _hook_api(sendto);

    ssize_t r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(sendto)(fd, buf, len, flags, addr, addrlen);
        goto end;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(fd, 1); ctx->set_nb_mark(); }
    {
        co::IoEvent ev(fd, co::ev_write);
        do_hook(__sys_api(sendto)(fd, buf, len, flags, addr, addrlen), ev, ctx->send_timeout());
    }

  end:
    HOOKLOG << "hook sendto, fd: " << fd << ", n: " << len << ", r: " << r;
    return r;
}

ssize_t _hook(sendmsg)(int fd, const struct msghdr* msg, int flags) {
    _hook_api(sendmsg);

    ssize_t r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(sendmsg)(fd, msg, flags);
        goto end;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(fd, 1); ctx->set_nb_mark(); }
    {
        co::IoEvent ev(fd, co::ev_write);
        do_hook(__sys_api(sendmsg)(fd, msg, flags), ev, ctx->send_timeout());
    }

  end:
    HOOKLOG << "hook sendmsg, fd: " << fd << ", r: " << r;
    return r;
}

int _hook(poll)(struct pollfd* fds, nfds_t nfds, int ms) {
    _hook_api(poll);

    int r, fd = nfds > 0 ? fds[0].fd : -1;
    uint32 t = ms < 0 ? -1 : ms, x = 1;
    if (!co::gSched || ms == 0) {
        r = __sys_api(poll)(fds, nfds, ms);
        goto end;
    }

    do {
        if (nfds == 1) {
            auto ctx = gHook().get_hook_ctx(fd);
            if (!ctx || !ctx->is_sock_or_pipe() || !ctx->is_non_blocking()) break;

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
            if (ms > 0 && co::gSched->timeout()) { r = 0; goto end; }

            fds[0].revents = fds[0].events;
            r = 1; goto end;
        }
    } while (0);

    if (nfds == 0 && t != -1) {
        co::gSched->sleep(t);
        goto end;
    }

    // just check poll every x ms
    do {
        r = __sys_api(poll)(fds, nfds, 0);
        if (r != 0 || t == 0) goto end;
        co::gSched->sleep(t > x ? x : t);
        if (t != -1) t = (t > x ? t - x : 0);
        if (x < 16) x <<= 1;
    } while (true);

  end:
    HOOKLOG << "hook poll, nfds: " << nfds << ", fd: " << fd << ", ms: " << ms << ", r: " << r;
    return r;
}

int _hook(select)(int nfds, fd_set* rs, fd_set* ws, fd_set* es, struct timeval* tv) {
    _hook_api(select);

    const int64 max_ms = ((uint32)-1) >> 1;
    int r, ms = -1;
    uint32 t, x = 1;
    int64 sec, us;

    if (tv) {
        sec = tv->tv_sec;
        us = tv->tv_usec;
        if (sec >= 0 && us >= 0) {
            if (sec < max_ms / 1000 && us < max_ms * 1000) {
                const int64 u = sec * 1000000 + us;
                ms = u <= 1000 ? !!u : (u < max_ms * 1000 ? u / 1000 : max_ms);
            } else {
                ms = (int)max_ms;
            }
        }
    } else {
        sec = us = 0;
    }

    if (!co::gSched || nfds < 0 || ms == 0 || sec < 0 || us < 0) {
        r = __sys_api(select)(nfds, rs, ws, es, tv);
        goto end;
    }

    if ((nfds == 0 || (!rs && !ws && !es)) && ms > 0) {
        co::gSched->sleep(ms);
        r = 0;
        goto end;
    }

    // just check select every x ms
    {
        struct timeval o = { 0, 0 };
        fd_set s[3];
        if (rs) s[0] = *rs;
        if (ws) s[1] = *ws;
        if (es) s[2] = *es;

        t = ms;
        do {
            r = __sys_api(select)(nfds, rs, ws, es, &o);
            if (r != 0 || t == 0) goto end;
            co::gSched->sleep(t > x ? x : t);
            if (t != -1) t = (t > x ? t - x : 0);
            if (x < 16) x <<= 1;
            if (rs) *rs = s[0];
            if (ws) *ws = s[1];
            if (es) *es = s[2];
            o.tv_sec = o.tv_usec = 0;
        } while (true);
    }

  end:
    HOOKLOG << "hook select, nfds: " << nfds << ", ms: " << ms << ", r: " << r;
    return r;
}

unsigned int _hook(sleep)(unsigned int n) {
    _hook_api(sleep);

    unsigned int r;
    if (!co::gSched || !gHook().hook_sleep) {
        r = __sys_api(sleep)(n);
        goto end;
    }

    co::gSched->sleep(n * 1000);
    r = 0;

  end:
    HOOKLOG << "hook sleep, sec: " << n << ", r: " << r;
    return r;
}

int _hook(usleep)(useconds_t us) {
    _hook_api(usleep);

    int r;
    if (us >= 1000000) { r = -1; errno = EINVAL; goto end; }

    if (!co::gSched || !gHook().hook_sleep) {
        r = __sys_api(usleep)(us);
        goto end;
    }

    co::gSched->sleep(us <= 0 ? 0 : (us <= 1000 ? 1 : us / 1000));
    r = 0;

  end:
    HOOKLOG << "hook usleep, us: " << us << ", r: " << r;
    return r;
}

int _hook(nanosleep)(const struct timespec* req, struct timespec* rem) {
    _hook_api(nanosleep);

    int r, ms = -1;
    if (req) {
        if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec > 999999999) {
            errno = EINVAL;
            r = -1;
            goto end;
        }

        const int64 max_ms = ((uint32)-1) >> 1;
        const int64 sec = req->tv_sec;
        if (sec < max_ms / 1000) {
            const int64 n = sec * 1000000000 + req->tv_nsec;
            ms = n <= 1000000 ? !!n : (n < max_ms * 1000000 ? n / 1000000 : max_ms);
        } else {
            ms = (int)max_ms;
        }
    }

    if (!co::gSched || !gHook().hook_sleep || !req) {
        r = __sys_api(nanosleep)(req, rem);
        goto end;
    }

    co::gSched->sleep(ms);
    r = 0;

  end:
    HOOKLOG << "hook nanosleep, ms: " << ms << ", r: " << r;
    return r;
}

#ifdef __linux__
int _hook(epoll_wait)(int epfd, struct epoll_event* events, int n, int ms) {
    _hook_api(epoll_wait);

    int r;
    if (!co::gSched || epfd < 0 || ms == 0) {
        r = __sys_api(epoll_wait)(epfd, events, n, ms);
        goto end;
    }

    {
        co::IoEvent ev(epfd, co::ev_read);
        if (!ev.wait(ms)) { r = 0; goto end; } // timeout
        r = __sys_api(epoll_wait)(epfd, events, n, 0);
    }

  end:
    HOOKLOG << "hook epoll_wait, fd: " << epfd << ", n: " << n << ", ms: " << ms << ", r: " << r;
    return r;
}

#ifdef SOCK_NONBLOCK
int _hook(accept4)(int fd, struct sockaddr* addr, socklen_t* addrlen, int flags) {
    _hook_api(accept4);

    int r;
    auto ctx = gHook().get_hook_ctx(fd);
    if (!co::gSched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(accept4)(fd, addr, addrlen, flags);
        goto end;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(fd, 1); ctx->set_nb_mark(); }
    {
        co::IoEvent ev(fd, co::ev_read);
        do {
            r = __sys_api(accept4)(fd, addr, addrlen, flags);
            if (r != -1) goto end;

            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                ev.wait();
            } else if (errno != EINTR) {
                goto end;
            }
        } while (true);
    }

  end:
    if (r != -1) {
        auto c = gHook().get_hook_ctx(r);
        c->set_sock_or_pipe();
        c->set_non_blocking(flags & SOCK_NONBLOCK);
    }
    HOOKLOG << "hook accept4, fd: " << fd << ", flags: " << flags << ", r: " << r;
    return r;
}
#endif

int _hook(gethostbyname_r)(
    const char* name,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    _hook_api(gethostbyname_r);
    HOOKLOG << "hook gethostbyname_r, name: " << (name ? name : "");
    if (!co::gSched) return __sys_api(gethostbyname_r)(name, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex_t());
    return __sys_api(gethostbyname_r)(name, ret, buf, len, res, err);
}

int _hook(gethostbyname2_r)(
    const char* name, int af,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    _hook_api(gethostbyname2_r);
    HOOKLOG << "hook gethostbyname2_r, name: " << (name ? name : "");
    if (!co::gSched) return __sys_api(gethostbyname2_r)(name, af, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex_t());
    return __sys_api(gethostbyname2_r)(name, af, ret, buf, len, res, err);
}

int _hook(gethostbyaddr_r)(
    const void* addr, socklen_t addrlen, int type,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    _hook_api(gethostbyaddr_r);
    HOOKLOG << "hook gethostbyaddr_r";
    if (!co::gSched) return __sys_api(gethostbyaddr_r)(addr, addrlen, type, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex_t());
    return __sys_api(gethostbyaddr_r)(addr, addrlen, type, ret, buf, len, res, err);
}

#ifdef NETDB_INTERNAL
struct hostent* _hook(gethostbyname2)(const char* name, int af) {
    _hook_api(gethostbyname2);
    HOOKLOG << "hook gethostbyname2, name: " << (name ? name : "");
    if (!co::gSched || !name) return __sys_api(gethostbyname2)(name, af);

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
int _hook(kevent)(int kq, const struct kevent* c, int nc, struct kevent* e, int ne, const struct timespec* ts) {
    _hook_api(kevent);

    int r, ms = -1, fd = -1;
    if (!co::gSched || c || kq < 0) {
        r = __sys_api(kevent)(kq, c, nc, e, ne, ts);
        goto end;
    }

    {
        if (ts) {
            const int64 max_ms = ((uint32)-1) >> 1;
            const int64 sec = ts->tv_sec;
            if (sec < max_ms / 1000) {
                const int64 n = sec * 1000000000 + ts->tv_nsec;
                ms = n <= 1000000 ? !!n : (n < max_ms * 1000000 ? n / 1000000 : max_ms);
            } else {
                ms = (int)max_ms;
            }
        }

        if (ms == 0) {
            r = __sys_api(kevent)(kq, c, nc, e, ne, ts);
            goto end;
        }

        {
            co::IoEvent ev(kq, co::ev_read);
            if (!ev.wait(ms)) { r = 0; goto end; }
            r = __sys_api(kevent)(kq, c, nc, e, ne, 0);
        }
    }

  end:
    if (!c) {
        if (r > 0) fd = (int)e->ident;
        HOOKLOG << "hook kevent, kq: " << kq << " ne: " << ne << " ms: " << ms << " fd: " << fd << " r: " << r;
    } else {
        fd = (int)c->ident;
        HOOKLOG << "hook kevent, kq: " << kq << " nc: " << nc << " fd: " << fd << " r: " << r;
    }
    return r;
}
#endif

struct hostent* _hook(gethostbyname)(const char* name) {
    _hook_api(gethostbyname);

    HOOKLOG << "hook gethostbyname, name: " << (name ? name : "");
    if (!co::gSched) return __sys_api(gethostbyname)(name);

    co::MutexGuard g(gDnsMutex_g());
    struct hostent* r = __sys_api(gethostbyname)(name);
    if (!r) return 0;

    struct hostent* ent = gHostEnt();
    *ent = *r;
    return ent;
}

struct hostent* _hook(gethostbyaddr)(const void* addr, socklen_t len, int type) {
    _hook_api(gethostbyaddr);

    HOOKLOG << "hook gethostbyaddr";
    if (!co::gSched) return __sys_api(gethostbyaddr)(addr, len, type);

    co::MutexGuard g(gDnsMutex_g());
    struct hostent* r = __sys_api(gethostbyaddr)(addr, len, type);
    if (!r) return 0;

    struct hostent* ent = gHostEnt();
    *ent = *r;
    return ent;
}

} // "C"

namespace co {

// DO NOT call _init_hooks() from outside on Linux.
static bool _init_hooks() {
  #ifdef __APPLE__
    int r, fds[2];
    if (!__sys_api(socket) && !__sys_api(close)) {
        r = ::socket(-1, -1, -1);
        r = ::socketpair(-1, -1, -1, 0);
        r = ::pipe(fds);
        r = ::close(fds[0]);
        r = ::close(fds[1]);
        r = ::fcntl(-1, -1, 0);
        r = ::ioctl(-1, -1, 0);
        r = ::dup(-1);
        r = ::dup2(-1, 0);
        r = ::setsockopt(-1, -1, -1, 0, 0);
        r = ::shutdown(-1, 0);
        r = ::connect(-1, 0, 0);
        r = ::accept(-1, 0, 0);
        r = ::read(-1, 0, 0);
        r = ::readv(-1, 0, 0);
        r = ::recv(-1, 0, 0, 0);
        r = ::recvfrom(-1, 0, 0, 0, 0, 0);
        r = ::recvmsg(-1, 0, 0);
        r = ::write(-1, 0, 0);
        r = ::writev(-1, 0, 0);
        r = ::send(-1, 0, 0, 0);
        r = ::sendto(-1, 0, 0, 0, 0, 0);
        r = ::sendmsg(-1, 0, 0);
        r = ::poll(0, 0, 0);
        r = ::select(-1, 0, 0, 0, 0);
        r = ::sleep(0);
        r = ::usleep(0);
        r = ::nanosleep(0, 0);
        r = ::kevent(-1, 0, 0, 0, 0, 0);
        auto x = ::gethostbyaddr("0", 1, -1);
        x = ::gethostbyname("localhost");
        (void)r; (void)x;
    }
  #endif

    hook_api(socket);
    hook_api(socketpair);
    hook_api(pipe);
    hook_api(fcntl);
    hook_api(ioctl);
    hook_api(dup);
    hook_api(dup2);
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
    hook_api(gethostbyaddr);
    hook_api(gethostbyname);

  #ifdef __linux__
    hook_api(dup3);
    hook_api(pipe2);
    hook_api(epoll_wait);
    hook_api(accept4);
    hook_api(gethostbyname2);
    hook_api(gethostbyname_r);
    hook_api(gethostbyname2_r);
    hook_api(gethostbyaddr_r);
  #else
    hook_api(kevent);
  #endif

  #ifdef __APPLE__
    // on 64 bit mac os, internal name of select may be "select$1050"
    if (!__sys_api(select)) {
        void* origin = 0;
        rebind_symbols((rebinding[1]){{"select$1050", (void*)::_hook(select), (void**)&origin}}, 1);
        if (!origin) COUT << "hook select failed";
        atomic_store(&__sys_api(select), origin, mo_relaxed);
    }
  #endif

    return true;
}

static bool _dummy = _init_hooks();

void init_hook() {
    (void)_dummy;
  #ifdef __APPLE__
    _init_hooks();
  #else
    // ensure the following APIs are hooked
    if (!__sys_api(close)) { (void) ::close(-1); }
    if (!__sys_api(read))  { auto r = ::read(-1, 0, 0);  (void)r; }
    if (!__sys_api(write)) { auto r = ::write(-1, 0, 0); (void)r; }
    if (!__sys_api(pipe))  { auto r = ::pipe((int*)0);   (void)r; }
    if (!__sys_api(fcntl)) { (void) ::fcntl(-1, 0); }
    CHECK(__sys_api(close) != 0);
    CHECK(__sys_api(read) != 0);
    CHECK(__sys_api(write) != 0);
    CHECK(__sys_api(pipe) != 0);
    CHECK(__sys_api(fcntl) != 0);
  #endif

  #ifdef __linux__
    if (__sys_api(epoll_wait) == 0) ::epoll_wait(-1, 0, 0, 0);
    CHECK(__sys_api(epoll_wait) != 0);
  #endif
}

void cleanup_hook() {
    atomic_store(&gHook().hook_sleep, false, mo_release);
}

void disable_hook_sleep() {
    atomic_store(&gHook().hook_sleep, false, mo_release);
}

void enable_hook_sleep() {
    atomic_store(&gHook().hook_sleep, true, mo_release);
}

} // co

#undef do_hook
#undef _hook
#undef _hook_api
#undef hook_api
#undef HOOKLOG

#endif
#endif
