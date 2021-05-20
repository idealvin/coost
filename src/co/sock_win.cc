#ifdef _WIN32

#include "co/co/sock.h"
#include "co/co/scheduler.h"
#include "co/co/io_event.h"
#include <ws2spi.h>

namespace co {
using xx::gSched;

LPFN_CONNECTEX connect_ex = 0;
LPFN_ACCEPTEX accept_ex = 0;
LPFN_GETACCEPTEXSOCKADDRS get_accept_ex_addrs = 0;

sock_t socket(int domain, int type, int protocol) {
    sock_t fd;
    if (type != SOCK_DGRAM) {
        fd = WSASocketW(domain, type, 0, 0, 0, WSA_FLAG_OVERLAPPED);
    } else {
        fd = WSASocketW(domain, SOCK_DGRAM, 0, 0, 0, WSA_FLAG_OVERLAPPED);
    }
    if (fd != INVALID_SOCKET) co::set_nonblock(fd);
    return fd;
}

int close(sock_t fd, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    gSched->del_io_event(fd);
    if (ms > 0) gSched->sleep(ms);
    return ::closesocket(fd);
}

int shutdown(sock_t fd, char c) {
    CHECK(gSched) << "must be called in coroutine..";
    if (c == 'r') {
        gSched->del_io_event(fd, EV_read);
        return ::shutdown(fd, SD_RECEIVE);
    } else if (c == 'w') {
        gSched->del_io_event(fd, EV_write);
        return ::shutdown(fd, SD_SEND);
    } else {
        gSched->del_io_event(fd);
        return ::shutdown(fd, SD_BOTH);
    }
}

int bind(sock_t fd, const void* addr, int addrlen) {
    return ::bind(fd, (const struct sockaddr*)addr, (socklen_t)addrlen);
}

int listen(sock_t fd, int backlog) {
    return ::listen(fd, backlog);
}

inline int find_address_family(sock_t fd) {
    static std::vector<std::unordered_map<sock_t, int>> kAf(xx::scheduler_num());

    auto& map = kAf[gSched->id()];
    int& af = map[fd];
    if (af != 0) return af;
    {
        WSAPROTOCOL_INFOW info;
        int len = sizeof(info);
        int r = co::getsockopt(fd, SOL_SOCKET, SO_PROTOCOL_INFO, &info, &len);
        if (r != 0) return -1;
        af = info.iAddressFamily;
    }
    return af;
}

sock_t accept(sock_t fd, void* addr, int* addrlen) {
    CHECK(gSched) << "must be called in coroutine..";

    // We have to figure out the address family of the listening socket here.
    int af = find_address_family(fd);
    if (af < 0) return (sock_t)-1;

    sock_t connfd = co::tcp_socket(af);
    if (connfd == INVALID_SOCKET) return connfd;

    const int N = sizeof(sockaddr_in6);
    IoEvent ev(fd, (N + 16) * 2);
    sockaddr *serv = 0, *peer = 0;
    int serv_len = N, peer_len = N;

    int r = accept_ex(fd, connfd, ev->s, 0, N + 16, N + 16, 0, &ev->ol);
    if (r == FALSE) {
        if (co::error() != ERROR_IO_PENDING) goto err;
        ev.wait();
    }

    // https://docs.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-acceptex
    r = co::setsockopt(connfd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, &fd, sizeof(fd));
    if (r != 0) {
        ELOG << "acceptex set SO_UPDATE_ACCEPT_CONTEXT failed..";
        goto err;
    }

    get_accept_ex_addrs(ev->s, 0, N + 16, N + 16, &serv, &serv_len, &peer, &peer_len);
    if (addr && addrlen) {
        if (peer_len <= *addrlen) memcpy(addr, peer, peer_len);
        *addrlen = peer_len;
    }

    if (connfd != INVALID_SOCKET) co::set_nonblock(fd);
    return connfd;

  err:
    ::closesocket(connfd);
    return -1;
}

int connect(sock_t fd, const void* addr, int addrlen, int ms) {
    CHECK(gSched) << "must be called in coroutine..";

    // docs.microsoft.com/zh-cn/windows/win32/api/mswsock/nc-mswsock-lpfn_connectex
    // stackoverflow.com/questions/13598530/connectex-requires-the-socket-to-be-initially-bound-but-to-what
    // @fd must be an unconnected, previously bound socket
    do {
        union {
            struct sockaddr_in  v4;
            struct sockaddr_in6 v6;
        } addr;
        memset(&addr, 0, sizeof(addr));

        if (addrlen == sizeof(sockaddr_in)) {
            addr.v4.sin_family = AF_INET;
        } else {
            addr.v6.sin6_family = AF_INET6;
        }

        if (co::bind(fd, &addr, addrlen) != 0) {
            ELOG << "connectex bind local addr failed..";
            return -1;
        }
    } while (0);

    IoEvent ev(fd);

    int r = connect_ex(fd, (const sockaddr*)addr, addrlen, 0, 0, 0, &ev->ol);
    if (r == FALSE) {
        if (co::error() != ERROR_IO_PENDING) return -1;
        if (!ev.wait(ms)) return -1;
    }

    r = co::setsockopt(fd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0);
    if (r != 0) {
        ELOG << "connectex set SO_UPDATE_ACCEPT_CONTEXT failed..";
        return -1;
    }

    int seconds;
    int len = sizeof(int);
    r = co::getsockopt(fd, SOL_SOCKET, SO_CONNECT_TIME, &seconds, &len);
    if (r == 0) {
        if (seconds == -1) return -1; // not connected
        return 0;
    } else {
        ELOG << "connectex getsockopt(SO_CONNECT_TIME) failed..";
        return -1;
    }
}

int recv(sock_t fd, void* buf, int n, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    IoEvent ev(fd, EV_read);

    do {
        int r = ::recv(fd, (char*)buf, n, 0);
        if (r != -1) return r;

        if (co::error() == WSAEWOULDBLOCK) {
            if (!ev.wait(ms)) return -1;
        } else {
            return -1;
        }
    } while (true);
}

int recvn(sock_t fd, void* buf, int n, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    char* s = (char*) buf;
    int remain = n;
    IoEvent ev(fd, EV_read);

    do {
        int r = ::recv(fd, s, remain, 0);
        if (r == remain) return n;
        if (r == 0) return 0;

        if (r == -1) {
            if (co::error() == WSAEWOULDBLOCK) {
                if (!ev.wait(ms)) return -1;
            } else {
                return -1;
            }
        } else {
            remain -= r;
            s += r;
        }
    } while (true);
}

int recvfrom(sock_t fd, void* buf, int n, void* addr, int* addrlen, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    int r;
    const int N = sizeof(sockaddr_in6) + 8;
    const bool on_stack = gSched->on_stack(buf);
    char* s = 0;

    IoEvent ev(fd, on_stack ? N + n : N);
    ev->buf.buf = (on_stack ? ev->s + N : (char*)buf);
    ev->buf.len = n;

    if (addr && addrlen) {
        s = ev->s;
        *(int*)s = sizeof(sockaddr_in6);
        r = WSARecvFrom(fd, &ev->buf, 1, &ev->n, &ev->flags, (sockaddr*)(s + 8), (int*)s, &ev->ol, 0);
    } else {
        r = WSARecvFrom(fd, &ev->buf, 1, &ev->n, &ev->flags, 0, 0, &ev->ol, 0);
    }

    if (r == 0) {
        ev.wait();
    } else if (co::error() == WSA_IO_PENDING) {
        if (!ev.wait(ms)) return -1;
    } else {
        return -1;
    }

    if (s) {
        const int peer_len = *(int*)s;
        if (peer_len <= *addrlen) memcpy(addr, s + 8, peer_len);
        *addrlen = peer_len;
    }

    if (on_stack) memcpy(buf, ev->s + N, ev->n);
    return (int)ev->n;
}

int send(sock_t fd, const void* buf, int n, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    const char* s = (const char*) buf;
    int remain = n;
    IoEvent ev(fd, EV_write);

    do {
        int r = ::send(fd, s, remain, 0);
        if (r == remain) return n;

        if (r == -1) {
            if (co::error() == WSAEWOULDBLOCK) {
                if (!ev.wait(ms)) return -1;
            } else {
                return -1;
            }
        } else {
            remain -= r;
            s += r;
        }
    } while (true);
}

int sendto(sock_t fd, const void* buf, int n, const void* addr, int addrlen, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    int r;
    const bool on_stack = gSched->on_stack(buf);

    IoEvent ev(fd, on_stack ? n : 0);
    ev->buf.buf = (on_stack ? ev->s : (char*)buf);
    ev->buf.len = n;
    if (on_stack) memcpy(ev->s, buf, n);

    while (true) {
        r = WSASendTo(fd, &ev->buf, 1, &ev->n, 0, (const sockaddr*)addr, addrlen, &ev->ol, 0);
        if (r == 0) {
            ev.wait();
        } else if (co::error() == WSA_IO_PENDING) {
            if (!ev.wait(ms)) return -1;
        } else {
            return -1;
        }

        if (ev->n == (DWORD)ev->buf.len) return n;
        if (ev->n == 0) return -1;
        ev->buf.buf += ev->n;
        ev->buf.len -= ev->n;
        memset(&ev->ol, 0, sizeof(ev->ol));
    }
}

class Error {
  public:
    Error() = default;
    ~Error() = default;

    struct T {
        T() : err(4096) {}
        fastream err;
        std::unordered_map<int, uint32> pos;
    };

    const char* strerror(int e) {
        if (_p == NULL) _p.reset(new T);
        auto it = _p->pos.find(e);
        if (it != _p->pos.end()) {
            return _p->err.data() + it->second;
        } else {
            uint32 pos = (uint32) _p->err.size();
            char* s = 0;
            FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                0, e,
                MAKELANGID(LANG_ENGLISH /*LANG_NEUTRAL*/, SUBLANG_DEFAULT),
                (LPSTR)&s, 0, 0
            );

            if (s) {
                _p->err.append(s).append('\0');
                LocalFree(s);
                char* p = (char*) strchr(_p->err.data() + pos, '\r');
                if (p) *p = '\0';
            } else {
                _p->err.append("Unknown error ");
                _p->err << e;
                _p->err.append('\0');
            }
            _p->pos[e] = pos;
            return _p->err.data() + pos;
        }
    }

  private:
    thread_ptr<T> _p;
};

const char* strerror(int err) {
    if (err == ETIMEDOUT) return "Timed out.";
    static co::Error e;
    return e.strerror(err);
}

namespace xx {

void wsa_startup() {
    WSADATA x;
    WSAStartup(MAKEWORD(2, 2), &x);

    sock_t fd = co::tcp_socket();
    CHECK_NE(fd, INVALID_SOCKET) << "create socket error: " << co::strerror();

    int r = 0;
    DWORD n = 0;
    GUID guid;

    guid = WSAID_CONNECTEX;
    r = WSAIoctl(
        fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guid, sizeof(guid),
        &connect_ex, sizeof(connect_ex),
        &n, 0, 0
    );
    CHECK_EQ(r, 0) << "get ConnectEx failed: " << co::strerror();

    guid = WSAID_ACCEPTEX;
    r = WSAIoctl(
        fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guid, sizeof(guid),
        &accept_ex, sizeof(accept_ex),
        &n, 0, 0
    );
    CHECK_EQ(r, 0) << "get AcceptEx failed: " << co::strerror();

    guid = WSAID_GETACCEPTEXSOCKADDRS;
    r = WSAIoctl(
        fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guid, sizeof(guid),
        &get_accept_ex_addrs, sizeof(get_accept_ex_addrs),
        &n, 0, 0
    );

    CHECK_EQ(r, 0) << "get GetAccpetExSockAddrs failed: " << co::strerror();
    ::closesocket(fd);
}

void wsa_cleanup() { WSACleanup(); }

} // xx
} // co

#endif // _WIN32
