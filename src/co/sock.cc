#include "co/sock.h"
#include "co/assert.h"
#include "co/error.h"
#include "sched.h"

#ifndef _WIN32
#include "../close.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>  // basic socket api, struct linger
#include <netinet/in.h>  // for struct sockaddr_in
#include <netinet/tcp.h> // for TCP_NODELAY...
#include <arpa/inet.h>   // for inet_ntop...
#include <netdb.h>       // getaddrinfo, gethostby...

#else
#include "co/log.h"
#include <WinSock2.h>
#include <ws2tcpip.h> // for inet_ntop...
#include <MSWSock.h>
#include <ws2spi.h>
#endif


namespace co {

struct alignas(8) _sockaddr {
    _sockaddr() { this->reset(); }
    _sockaddr(const char* host, uint16 port);

    void reset() { len = sizeof(p); }
    bool valid() const { return len != sizeof(p); }

    uint32 p[8]; // store sockaddr_in or sockaddr_in6
    union {
        socklen_t len;
        uint32 _[2];
    };
};

static_assert(sizeof(sockaddr_in) < 32, "");
static_assert(sizeof(sockaddr_in6) < 32, "");
static_assert(alignof(sockaddr_in) <= alignof(co::sockaddr), "");
static_assert(alignof(sockaddr_in6) <= alignof(co::sockaddr), "");
static_assert(sizeof(co::sockaddr) == sizeof(co::_sockaddr), "");
static_assert(alignof(co::sockaddr) == alignof(co::_sockaddr), "");

_sockaddr::_sockaddr(const char* host, uint16 port) {
    uint64 s_port = 0;
    co::itoa((uint16)port, (char*)&s_port, sizeof(s_port));

    ::addrinfo *ai = 0, *rp;
    int r = ::getaddrinfo(host, (const char*)&s_port, nullptr, &ai);
    if (r == 0) {
        for (rp = ai; rp; rp = rp->ai_next) {
            if (rp->ai_family == AF_INET) break;
        }
        if (!rp) rp = ai;

        len = (socklen_t)rp->ai_addrlen;
        runtime_assert(len < sizeof(p));
        ::memcpy(this, rp->ai_addr, len);
        ::freeaddrinfo(ai);

    } else {
        runtime_assert(r != EAI_MEMORY, "out of memory");
        this->reset();
    }
}

sockaddr::sockaddr() {
    new (this) _sockaddr();
}

sockaddr::sockaddr(const char* host, uint16 port) {
    new (this) _sockaddr(host, port);
}

bool sockaddr::valid() const {
    return ((const _sockaddr*)this)->valid();
}

int sockaddr::af() const {
    return ((const ::sockaddr*)this)->sa_family;
}

co::string& operator<<(co::string& s, const co::sockaddr& addr) {
    const auto paddr = (const _sockaddr*) &addr;
    if (paddr->valid()) {
        if (paddr->len == sizeof(::sockaddr_in)) {
            auto a = (::sockaddr_in*) paddr;
            char p[INET_ADDRSTRLEN] = { 0 };
            ::inet_ntop(AF_INET, (void*)&a->sin_addr, p, sizeof(p));
            s << p << ':' << co::ntoh16(a->sin_port);
        } else if (paddr->len == sizeof(::sockaddr_in6)) {
            auto a = (::sockaddr_in6*) paddr;
            char p[INET6_ADDRSTRLEN] = { 0 };
            ::inet_ntop(AF_INET6, (void*)&a->sin6_addr, p, sizeof(p));
            s << p << ':' << co::ntoh16(a->sin6_port);
        }
    }
    return s;
}

co::sockaddr peeraddr(sock_t fd) {
    co::sockaddr addr;
    co::_sockaddr* paddr = (co::_sockaddr*)&addr;
    const int r = ::getpeername(fd, (::sockaddr*)paddr, &paddr->len);
    if (r != 0) paddr->reset();
    return addr;
}

#ifndef _WIN32
inline int getsockopt(sock_t fd, int lv, int opt, void* optval, int* optlen) {
    if (sizeof(socklen_t) == sizeof(int)) {
        return ::getsockopt(fd, lv, opt, optval, (socklen_t*)optlen);
    } else {
        socklen_t x = *optlen;
        int r = ::getsockopt(fd, lv, opt, optval, &x);
        if (r == 0) *optlen = (int)x;
        return r;
    }
}

inline int setsockopt(sock_t fd, int lv, int opt, const void* optval, int optlen) {
    return ::setsockopt(fd, lv, opt, optval, (socklen_t)optlen);
}

void set_nonblock(sock_t fd) {
    ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
}

void set_cloexec(sock_t fd) {
    ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC);
}

#ifdef SOCK_NONBLOCK
inline sock_t socket(int domain, int type, int protocol) {
    return ::socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
}

#else
inline sock_t socket(int domain, int type, int protocol) {
    sock_t fd = ::socket(domain, type, protocol);
    if (fd != -1) {
        co::set_nonblock(fd);
        co::set_cloexec(fd);
    }
    return fd;
}
#endif // ifdef SOCK_NONBLOCK

#else /* _WIN32 */
LPFN_CONNECTEX g_connectex = 0;
LPFN_ACCEPTEX g_acceptex = 0;
LPFN_GETACCEPTEXSOCKADDRS g_get_acceptex_addrs = 0;
bool g_can_skip_iocp_on_success = false;

inline int getsockopt(sock_t fd, int lv, int opt, void* optval, int* optlen) {
    const int r = ::getsockopt(fd, lv, opt, (char*)optval, optlen);
    if (r != 0) co::error(WSAGetLastError());
    return r;
}

inline int setsockopt(sock_t fd, int lv, int opt, const void* optval, int optlen) {
    int r = ::setsockopt(fd, lv, opt, (const char*)optval, optlen);
    if (r != 0) co::error(WSAGetLastError());
    return r;
}

void set_nonblock(sock_t fd) {
   unsigned long mode = 1;
   ::ioctlsocket(fd, FIONBIO, &mode);
}

inline void set_skip_iocp_on_success(sock_t fd) {
    if (g_can_skip_iocp_on_success) {
        SetFileCompletionNotificationModes(
            (HANDLE)fd, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS
        );
    }
}

sock_t socket(int domain, int type, int protocol) {
    static_assert(INVALID_SOCKET == (sock_t)-1, "");
    sock_t fd = ::WSASocketW(domain, type, protocol, 0, 0, WSA_FLAG_OVERLAPPED);
    if (fd != (sock_t)-1) {
        co::set_nonblock(fd);
        if (type != SOCK_STREAM) set_skip_iocp_on_success(fd);
    } else {
        co::error(WSAGetLastError());
    }
    return fd;
}

#endif // ifndef _WIN32

void set_reuseaddr(sock_t fd) {
    const int v = 1;
    co::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
}

void set_tcp_nodelay(sock_t fd) {
    const int v = 1;
    co::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v));
}

void set_tcp_keepalive(sock_t fd) {
    const int v = 1;
    co::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &v, sizeof(v));
}

void set_send_buffer_size(sock_t fd, int n) {
    co::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n));
}

void set_recv_buffer_size(sock_t fd, int n) {
    co::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));
}

int reset_tcp_socket(sock_t fd, int ms) {
    struct linger v = { 1, 0 };
    co::setsockopt(fd, SOL_SOCKET, SO_LINGER, &v, sizeof(v));
    return co::close(fd, ms);
}

sock_t tcp_server_socket(const char* host, uint16 port, int backlog) {
    co::sockaddr addr(host, port);
    co::_sockaddr* paddr = (co::_sockaddr*) &addr;
    if (!paddr->valid()) {
        co::error(EINVAL);
        return (sock_t)-1;
    }

    ::sockaddr& a = (::sockaddr&)addr;
    sock_t fd = co::socket(a.sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (fd != (sock_t)-1) {
        co::set_reuseaddr(fd);

        // turn off IPV6_V6ONLY
        if (a.sa_family == AF_INET6) {
            int x = 0;
            co::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &x, sizeof(x));
        }

        if (co::bind(fd, addr) != 0 || co::listen(fd, backlog) != 0) {
            const int e = co::error();
            co::close(fd);
            fd = (sock_t)-1;
            co::error(e);
        }
    }

    return fd;
}

#ifndef _WIN32
io_event::~io_event() {
    if (_added) g_sched->del_io_event(_fd, _ev);
}

bool io_event::wait(uint32 ms) {
    auto s = g_sched;
    if (!_added) {
        _added = s->add_io_event(_fd, _ev);
        if (!_added) return false;
    }

    if (ms != (uint32)-1) {
        s->add_timer(ms);
        s->yield();
        if (!s->timeout()) return true;
        errno = ETIMEDOUT;
        return false;
    } else {
        s->yield();
        return true;
    }
}

sock_t tcp_socket(int af) {
    return co::socket(af != 0 ? af : AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

sock_t udp_socket(int af) {
    return co::socket(af != 0 ? af : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

int close(sock_t fd, int ms) {
    if (fd < 0) return 0;
    const auto s = g_sched;
    if (s) {
        s->del_io_event(fd);
        if (ms > 0) s->sleep(ms);
    }
    return _close(fd);
}

int bind(sock_t fd, const co::sockaddr& addr) {
    const auto paddr = (const co::_sockaddr*) &addr;
    return ::bind(fd, (const ::sockaddr*)paddr, paddr->len);
}

int listen(sock_t fd, int backlog) {
    return ::listen(fd, backlog);
}

sock_t accept(sock_t fd, co::sockaddr* addr) {
    runtime_assert(g_sched, "must be called in coroutine");

    auto paddr = (co::_sockaddr*) addr;
    io_event ev(fd, ev_read);
    do {
        if (paddr && paddr->valid()) paddr->reset();

    #ifdef SOCK_NONBLOCK
        sock_t connfd = ::accept4(
            fd, (::sockaddr*)paddr, paddr ? &paddr->len : nullptr,
            SOCK_NONBLOCK | SOCK_CLOEXEC
        );
        if (connfd != -1) return connfd;
    #else
        sock_t connfd = ::accept(
            fd, (::sockaddr*)paddr, paddr ? &paddr->len : nullptr
        );
        if (connfd != -1) {
            co::set_nonblock(connfd);
            co::set_cloexec(connfd);
            return connfd;
        }
    #endif

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            ev.wait();
        } else if (errno != EINTR) {
            if (paddr) paddr->reset();
            return -1;
        }
    } while (true);
}

int connect(sock_t fd, const co::sockaddr& addr, int ms) {
    runtime_assert(g_sched, "must be called in coroutine");

    const auto paddr = (const co::_sockaddr*) &addr;
    do {
        int r = ::connect(fd, (const ::sockaddr*)paddr, paddr->len);
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
    runtime_assert(g_sched, "must be called in coroutine");

    io_event ev(fd, ev_read);
    do {
        int r = (int) ::recv(fd, buf, n, 0);
        if (r != -1) return r;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            if (!ev.wait(ms)) return -1;
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int recvn(sock_t fd, void* buf, int n, int ms) {
    runtime_assert(g_sched, "must be called in coroutine");

    char* p = (char*) buf;
    int remain = n;
    io_event ev(fd, ev_read);
    do {
        int r = (int) ::recv(fd, p, remain, 0);
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

int recvfrom(sock_t fd, void* buf, int n, co::sockaddr& addr, int ms) {
    runtime_assert(g_sched, "must be called in coroutine");

    auto paddr = (co::_sockaddr*) &addr;
    io_event ev(fd, ev_read);
    do {
        if (paddr->valid()) paddr->reset();

        int r = (int) ::recvfrom(
            fd, buf, n, 0, (::sockaddr*)paddr, &paddr->len
        );
        if (r != -1) return r;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            if (!ev.wait(ms)) return -1;
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int send(sock_t fd, const void* buf, int n, int ms) {
    runtime_assert(g_sched, "must be called in coroutine");

    const char* p = (const char*) buf;
    int remain = n;
    io_event ev(fd, ev_write);

    do {
        int r = (int) ::send(fd, p, remain, 0);
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

int sendto(sock_t fd, const void* buf, int n, const co::sockaddr& addr, int ms) {
    runtime_assert(g_sched, "must be called in coroutine");

    const auto paddr = (const co::_sockaddr*) &addr;
    const char* p = (const char*) buf;
    int remain = n;
    io_event ev(fd, ev_write);

    do {
        int r = (int) ::sendto(
            fd, p, remain, 0, (const ::sockaddr*)paddr, paddr->len
        );
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

#else /* _WIN32 */

io_event::io_event(sock_t fd, ev_t ev)
    : _fd(fd), _ev(ev), _type(io_tcp_nb), _copy_to(0) {
    const auto s = g_sched;
    s->add_io_event(fd, ev); // bind socket to IOCP
    _per_io = per_io_t::create(s->running());
}

io_event::io_event(sock_t fd, int n)
    : _fd(fd), _ev(ev_read), _type(io_conn), _copy_to(0) {
    const auto s = g_sched;
    s->add_io_event(fd, ev_read); // bind socket to IOCP
    _per_io = per_io_t::create(s->running(), n);
}

io_event::io_event(sock_t fd, ev_t ev, const void* buf, int size, int n)
    : _fd(fd), _ev(ev), _type(io_udp), _copy_to(nullptr) {
    const auto s = g_sched;
    s->add_io_event(fd, ev); // bind socket to IOCP

    if (!s->on_stack(buf)) {
        auto p = per_io_t::create(s->running(), n);
        p->buf.buf = (char*)buf;
        p->buf.len = size;
        _per_io = (void*)p;
    } else {
        auto p = per_io_t::create(s->running(), n, size);
        p->buf.buf = p->s + n;
        p->buf.len = size;
        if (ev == ev_read) {
            _copy_to = (void*)buf;
        } else {
            memcpy(p->buf.buf, buf, size);
        }
        _per_io = (void*)p;
    }
}

io_event::~io_event() {
    const auto s = g_sched;
    auto p = (per_io_t*)_per_io;
    if (!s->timeout()) {
        if (_copy_to && p->n > 0) memcpy(_copy_to, p->buf.buf, p->n);
        co::free(p, p->mlen);
    }
    s->running()->wtx = 0;
}

bool io_event::wait(uint32 ms) {
    int r, e;
    const auto s = g_sched;
    per_io_t& p = *(per_io_t*)_per_io;

    // If fd is a non-blocking TCP socket, we post the I/O operation to IOCP using 
    // WSARecv or WSASend. Since p.buf is empty, no data will be transfered, 
    // but we'll know that the socket is readable or writable when IOCP completes.
    if (_type == io_tcp_nb) {
        if (_ev == ev_read) {
            r = WSARecv(_fd, &p.buf, 1, &p.n, &p.flags, &p.ol, 0);
            if (r == 0 && g_can_skip_iocp_on_success) return true;
            if (r == SOCKET_ERROR) {
                e = WSAGetLastError();
                if (e != WSA_IO_PENDING) goto err;
            }
        } else {
            r = WSASend(_fd, &p.buf, 1, &p.n, 0, &p.ol, 0);
            if (r == 0 && g_can_skip_iocp_on_success) return true;
            if (r == SOCKET_ERROR) {
                e = WSAGetLastError();
                if (e == WSAENOTCONN) goto wait_for_connect; // the socket is not connected yet
                if (e != WSA_IO_PENDING) goto err;
            }
        }
    }

    if (ms != (uint32)-1) {
        s->add_timer(ms);
        s->yield();
        if (!s->timeout()) return true;

        CancelIo((HANDLE)_fd);
        //WSASetLastError(WSAETIMEDOUT);
        co::error(ETIMEDOUT);
        return false;
    } else {
        s->yield();
        return true;
    }

err:
    co::error(e);
    return false;

wait_for_connect:
    {
        // as no IO operation was posted to IOCP, remove waitx from coroutine
        s->running()->wtx = 0;

        // check if the socket is connected every x ms
        uint32 x = 1;
        int r, sec = 0, len = sizeof(int);
        while (true) {
            r = co::getsockopt(_fd, SOL_SOCKET, SO_CONNECT_TIME, &sec, &len);
            if (r != 0) return false;
            if (sec >= 0) return true; // connect ok
            if (ms == 0) {
                //WSASetLastError(WSAETIMEDOUT);
                co::error(ETIMEDOUT);
                return false;
            }
            s->sleep(ms > x ? x : ms);
            if (ms != (uint32)-1) ms = (ms > x ? ms - x : 0);
            if (x < 16) x <<= 1;
        }
    }
}

sock_t tcp_socket(int af) {
    return co::socket(af != 0 ? af : AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

sock_t udp_socket(int af) {
    return co::socket(af != 0 ? af : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

int close(sock_t fd, int ms) {
    if (fd == (sock_t)-1) return 0;

    const auto s = g_sched;
    if (s && ms > 0) s->sleep(ms);

    int r = ::closesocket(fd);
    if (r != 0) co::error(WSAGetLastError());
    return r;
}

int bind(sock_t fd, const co::sockaddr& addr) {
    const auto paddr = (const co::_sockaddr*) &addr;
    int r = ::bind(fd, (const ::sockaddr*)paddr, paddr->len);
    if (r != 0) co::error(WSAGetLastError());
    return r;
}

int listen(sock_t fd, int backlog) {
    int r = ::listen(fd, backlog);
    if (r != 0) co::error(WSAGetLastError());
    return r;
}

sock_t accept(sock_t fd, co::sockaddr* addr) {
    runtime_assert(g_sched, "must be called in coroutine");

    if (fd == (sock_t)-1) {
        co::error(WSAENOTSOCK);
        return (sock_t)-1;
    }

    static struct {
        sock_t fd;
        int af;
    } g_sock = { fd, -1 };

    // get the address family of the listening socket
    const int af = [](sock_t fd) {
        if (fd == g_sock.fd && g_sock.af != -1) return g_sock.af;

        WSAPROTOCOL_INFOW info;
        int len = sizeof(info);
        int r = co::getsockopt(fd, SOL_SOCKET, SO_PROTOCOL_INFO, &info, &len);
        if (r == 0) {
            if (fd == g_sock.fd) g_sock.af = info.iAddressFamily;
            return info.iAddressFamily;
        }

        log::error("failed to get address family of the listening socket: ", fd);
        return AF_INET;
    }(fd);

    sock_t connfd = co::socket(af, SOCK_STREAM, IPPROTO_TCP);
    if (connfd == (sock_t)-1) return connfd;

    const int N = sizeof(sockaddr_in6);
    io_event ev(fd, (N + 16) * 2);
    ::sockaddr *serv = 0, *peer = 0;
    auto paddr = (co::_sockaddr*)addr;
    int serv_len = N, peer_len = N, r, e;
    per_io_t& p = *(per_io_t*)ev._per_io;

    r = g_acceptex(fd, connfd, p.s, 0, N + 16, N + 16, 0, &p.ol);
    if (r == FALSE) {
        e = WSAGetLastError();
        if (e != ERROR_IO_PENDING) goto err;
        ev.wait();
    }

    // https://docs.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-acceptex
    r = ::setsockopt(connfd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&fd, sizeof(fd));
    if (r != 0) {
        e = WSAGetLastError();
        log::error("acceptex set SO_UPDATE_ACCEPT_CONTEXT failed, sock: ", connfd);
        goto err;
    }

    g_get_acceptex_addrs(p.s, 0, N + 16, N + 16, &serv, &serv_len, &peer, &peer_len);
    if (paddr) {
        if (peer_len == sizeof(sockaddr_in) || peer_len == sizeof(sockaddr_in6)) {
            memcpy(paddr, peer, peer_len);
            paddr->len = peer_len;
        } else {
            paddr->reset();
        }
    }

    set_skip_iocp_on_success(connfd);
    return connfd;

err:
    closesocket(connfd);
    co::error(e);
    return (sock_t)-1;
}

int connect(sock_t fd, const co::sockaddr& addr, int ms) {
    runtime_assert(g_sched, "must be called in coroutine");

    // docs.microsoft.com/zh-cn/windows/win32/api/mswsock/nc-mswsock-lpfn_connectex
    // stackoverflow.com/questions/13598530/connectex-requires-the-socket-to-be-initially-bound-but-to-what
    // @fd must be an unconnected, previously bound socket
    do {
        co::sockaddr a;
        ::memset(&a, 0, sizeof(sockaddr_in6));
        ((::sockaddr&)a).sa_family = addr.af();

        // WSAEINVAL is returned if the socket s is already bound to an address.
        if (co::bind(fd, a) != 0) {
            const int e = WSAGetLastError();
            if (e != WSAEINVAL) {
                log::error("connectex bind local address failed, sock: ", fd);
                co::error(e);
                return -1;
            }
        }
    } while (0);

    io_event ev(fd);
    int opt, len = sizeof(int), r, e;
    per_io_t* p = (per_io_t*)ev._per_io;
    const auto paddr = (const co::_sockaddr*)&addr;

    r = g_connectex(fd, (const ::sockaddr*)paddr, paddr->len, 0, 0, 0, &p->ol);
    if (r == FALSE) {
        e = WSAGetLastError();
        if (e != ERROR_IO_PENDING) goto err;
        if (!ev.wait(ms)) return -1;
    }

    r = ::setsockopt(fd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0);
    if (r != 0) {
        e = WSAGetLastError();
        log::error("connectex set SO_UPDATE_CONNECT_CONTEXT failed, sock: ", fd);
        goto err;
    }

    r = ::getsockopt(fd, SOL_SOCKET, SO_CONNECT_TIME, (char*)&opt, &len);
    if (r == 0) {
        // This is not likely to happen. If it happens, we still returns success,
        // as we'll wait for the connection to be completed in later sending ops.
        if (opt < 0) {
            log::error("connectex getsockopt(SO_CONNECT_TIME), opt < 0, sock: ", fd);
        }

        set_skip_iocp_on_success(fd);
        return 0;

    } else {
        e = WSAGetLastError();
        log::error("connectex getsockopt(SO_CONNECT_TIME) failed, sock: ", fd);
        goto err;
    }

err:
    co::error(e);
    return -1;
}

int recv(sock_t fd, void* buf, int n, int ms) {
    runtime_assert(g_sched, "must be called in coroutine");

    int r, e;
    io_event ev(fd, ev_read);
    do {
        r = ::recv(fd, (char*)buf, n, 0);
        if (r != -1) return r;

        e = WSAGetLastError();
        if (e == WSAEWOULDBLOCK) {
            if (!ev.wait(ms)) return -1;
        } else {
            co::error(e);
            return -1;
        }
    } while (true);
}

int recvn(sock_t fd, void* buf, int n, int ms) {
    runtime_assert(g_sched, "must be called in coroutine");

    char* p = (char*)buf;
    int remain = n, r, e;
    io_event ev(fd, ev_read);

    do {
        r = ::recv(fd, p, remain, 0);
        if (r == remain) return n;
        if (r == 0) return 0;

        if (r == -1) {
            e = WSAGetLastError();
            if (e == WSAEWOULDBLOCK) {
                if (!ev.wait(ms)) return -1;
            } else {
                co::error(e);
                return -1;
            }
        } else {
            remain -= r;
            p += r;
        }
    } while (true);
}

int recvfrom(sock_t fd, void* buf, int n, co::sockaddr& addr, int ms) {
    runtime_assert(g_sched, "must be called in coroutine");

    int r, e;
    char* s = 0;
    const int N = sizeof(sockaddr_in6) + 8;
    io_event ev(fd, ev_read, buf, n, N);
    per_io_t& p = *(per_io_t*)ev._per_io;
    auto paddr = (co::_sockaddr*)&addr;

    s = p.s;
    *(int*)s = sizeof(sockaddr_in6);
    r = WSARecvFrom(
        fd, &p.buf, 1, &p.n, &p.flags,
        (::sockaddr*)(s + 8), (int*)s, &p.ol, 0
    );

    if (r == 0) {
        if (!g_can_skip_iocp_on_success) ev.wait();
    } else {
        e = WSAGetLastError();
        if (e == WSA_IO_PENDING) {
            if (!ev.wait(ms)) return -1;
        } else {
            co::error(e);
            return -1;
        }
    }

    const unsigned int x = *(unsigned int*)s;
    runtime_assert(x <= sizeof(sockaddr_in6));
    memcpy(&addr, s + 8, x);
    paddr->len = x;
    return (int)p.n;
}

int send(sock_t fd, const void* buf, int n, int ms) {
    runtime_assert(g_sched, "must be called in coroutine");

    const char* p = (const char*)buf;
    int remain = n, r, e;
    io_event ev(fd, ev_write);

    do {
        r = ::send(fd, p, remain, 0);
        if (r == remain) return n;

        if (r == -1) {
            e = WSAGetLastError();
            if (e == WSAEWOULDBLOCK) {
                if (!ev.wait(ms)) return -1;
            } else {
                co::error(e);
                return -1;
            }
        } else {
            remain -= r;
            p += r;
        }
    } while (true);
}

int sendto(sock_t fd, const void* buf, int n, const co::sockaddr& addr, int ms) {
    runtime_assert(g_sched, "must be called in coroutine");

    int r, e;
    io_event ev(fd, ev_write, buf, n);
    per_io_t& p = *(per_io_t*)ev._per_io;
    const auto paddr = (const co::_sockaddr*)&addr;

    do {
        r = WSASendTo(
            fd, &p.buf, 1, &p.n, 0,
            (const ::sockaddr*)paddr, paddr->len, &p.ol, 0
        );
        if (r == 0) {
            if (!g_can_skip_iocp_on_success) ev.wait();
        } else {
            e = WSAGetLastError();
            if (e == WSA_IO_PENDING) {
                if (!ev.wait(ms)) return -1;
            } else {
                co::error(e);
                return -1;
            }
        }

        if (p.n == (DWORD)p.buf.len) return n;

        runtime_assert(p.n != 0);
        runtime_assert(p.n < (DWORD)p.buf.len);
        p.buf.buf += p.n;
        p.buf.len -= p.n;
        memset(&p.ol, 0, sizeof(p.ol));
    } while (true);
}

namespace xx {

static int g_nifty_counter;

SockInit::SockInit() {
    if (g_nifty_counter++ != 0) return;

    WSADATA x;
    WSAStartup(MAKEWORD(2, 2), &x);

    sock_t fd = ::socket(AF_INET, SOCK_STREAM, 0);
    int r = 0;
    DWORD n = 0;
    GUID guid;

    guid = WSAID_CONNECTEX;
    r = WSAIoctl(
        fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guid, sizeof(guid),
        &g_connectex, sizeof(g_connectex),
        &n, 0, 0
    );
    runtime_assert(r == 0, "get ConnectEx failed");

    guid = WSAID_ACCEPTEX;
    r = WSAIoctl(
        fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guid, sizeof(guid),
        &g_acceptex, sizeof(g_acceptex),
        &n, 0, 0
    );
    runtime_assert(r == 0, "get AcceptEx failed");

    guid = WSAID_GETACCEPTEXSOCKADDRS;
    r = WSAIoctl(
        fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guid, sizeof(guid),
        &g_get_acceptex_addrs, sizeof(g_get_acceptex_addrs),
        &n, 0, 0
    );
    runtime_assert(r == 0, "get GetAccpetExSockAddrs failed");

    ::closesocket(fd);

    g_can_skip_iocp_on_success = []() {
        int protos[] = { IPPROTO_TCP, IPPROTO_UDP, 0 };
        co::string s;
        LPWSAPROTOCOL_INFOW proto_info = 0;
        DWORD buf_len = 0;
        int err = 0, ntry = 0;
        int r = WSCEnumProtocols(&protos[0], proto_info, &buf_len, &err);
        runtime_assert(r == SOCKET_ERROR);

        while (true) {
            if (++ntry > 3) return false;
            s.reserve(buf_len);
            proto_info = (LPWSAPROTOCOL_INFOW)s.data();
            r = WSCEnumProtocols(&protos[0], proto_info, &buf_len, &err);
            if (r != SOCKET_ERROR) break;
        }

        for (int i = 0; i < r; ++i) {
            if (!(proto_info[i].dwServiceFlags1 & XP1_IFS_HANDLES)) return false;
        }
        return true;
    }();
}

SockInit::~SockInit() {
    if (--g_nifty_counter == 0) {
        WSACleanup();
    }
}

} // xx

#endif

} // co
