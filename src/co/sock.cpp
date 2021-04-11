#ifdef _WIN32

#include "co/co.h"
#include <ws2spi.h>

namespace co {

LPFN_CONNECTEX connect_ex = 0;
LPFN_ACCEPTEX accept_ex = 0;
LPFN_GETACCEPTEXSOCKADDRS get_accept_ex_addrs = 0;
bool can_skip_iocp_on_success = false;

inline void set_skip_iocp_on_success(sock_t fd) {
    if (can_skip_iocp_on_success) {
        SetFileCompletionNotificationModes((HANDLE)fd, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);
    }
}

sock_t socket(int domain, int type, int protocol) {
    if (type != SOCK_DGRAM) {
        return WSASocketW(domain, type, 0, 0, 0, WSA_FLAG_OVERLAPPED);
    } else {
        sock_t fd = WSASocketW(domain, SOCK_DGRAM, 0, 0, 0, WSA_FLAG_OVERLAPPED);
        if (fd != INVALID_SOCKET) set_skip_iocp_on_success(fd);
        return fd;
    }
}

int close(sock_t fd, int ms) {
    CHECK(scheduler()) << "must be called in coroutine..";
    scheduler()->del_io_event(fd);
    if (ms > 0) scheduler()->sleep(ms);
    return ::closesocket(fd);
}

int shutdown(sock_t fd, char c) {
    CHECK(scheduler()) << "must be called in coroutine..";
    if (c == 'r') {
        scheduler()->del_io_event(fd, EV_read);
        return ::shutdown(fd, SD_RECEIVE);
    } else if (c == 'w') {
        scheduler()->del_io_event(fd, EV_write);
        return ::shutdown(fd, SD_SEND);
    } else {
        scheduler()->del_io_event(fd);
        return ::shutdown(fd, SD_BOTH);
    }
}

int bind(sock_t fd, const void* addr, int addrlen) {
    return ::bind(fd, (const struct sockaddr*)addr, (socklen_t)addrlen);
}

int listen(sock_t fd, int backlog) {
    return ::listen(fd, backlog);
}

static int find_address_family(sock_t fd) {
    static std::vector<std::unordered_map<sock_t, int>> kAf(co::max_sched_num());

    auto& map = kAf[scheduler()->id()];
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
    CHECK(scheduler()) << "must be called in coroutine..";

    // We have to figure out the address family of the listening socket here.
    int af = find_address_family(fd);
    if (af < 0) return (sock_t)-1;

    sock_t connfd = co::tcp_socket(af);
    if (connfd == INVALID_SOCKET) return connfd;

    const int N = sizeof(sockaddr_in6);
    IoEvent ev(fd, EV_read, (N + 16) * 2);
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

    set_skip_iocp_on_success(connfd);
    return connfd;

  err:
    ::closesocket(connfd);
    return -1;
}

int connect(sock_t fd, const void* addr, int addrlen, int ms) {
    CHECK(scheduler()) << "must be called in coroutine..";

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

    IoEvent ev(fd, EV_write);

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
        set_skip_iocp_on_success(fd);
        return 0;
    } else {
        ELOG << "connectex getsockopt(SO_CONNECT_TIME) failed..";
        return -1;
    }
}

int recv(sock_t fd, void* buf, int n, int ms) {
    CHECK(scheduler()) << "must be called in coroutine..";
    IoEvent ev(fd, EV_read, buf, n);

    int r = WSARecv(fd, &ev->buf, 1, &ev->n, &ev->flags, &ev->ol, 0);
    if (r == 0) {
        if (!can_skip_iocp_on_success) ev.wait();
    } else if (co::error() == WSA_IO_PENDING) {
        if (!ev.wait(ms)) return -1;
    } else {
        return -1;
    }

    return (int) ev->n;
}

int recvn(sock_t fd, void* buf, int n, int ms) {
    IoEvent ev(fd, EV_read, buf, n);

    do {
        int r = WSARecv(fd, &ev->buf, 1, &ev->n, &ev->flags, &ev->ol, 0);
        if (r == 0) {
            if (!can_skip_iocp_on_success) ev.wait();
        } else if (co::error() == WSA_IO_PENDING) {
            if (!ev.wait(ms)) return -1;
        } else {
            return -1;
        }

        do {
            if (ev->n == (DWORD) ev->buf.len) return n;
            if (ev->n == 0) return 0; // connection closed
            ev->buf.buf += ev->n;
            ev->buf.len -= ev->n;
            memset(&ev->ol, 0, sizeof(ev->ol));
        } while (0);
    } while (true);
}

int recvfrom(sock_t fd, void* buf, int n, void* addr, int* addrlen, int ms) {
    CHECK(scheduler()) << "must be called in coroutine..";
    int r;
    char* s = 0;
    IoEvent ev(fd, EV_read, buf, n, sizeof(sockaddr_in6) + 8);

    if (addr && addrlen) {
        s = ev->s;
        *(int*)s = sizeof(sockaddr_in6);
        r = WSARecvFrom(fd, &ev->buf, 1, &ev->n, &ev->flags, (sockaddr*)(s + 8), (int*)s, &ev->ol, 0);
    } else {
        r = WSARecvFrom(fd, &ev->buf, 1, &ev->n, &ev->flags, 0, 0, &ev->ol, 0);
    }

    if (r == 0) {
        if (!can_skip_iocp_on_success) ev.wait();
    } else if (co::error() == WSA_IO_PENDING) {
        if (!ev.wait(ms)) return -1;
    } else {
        return -1;
    }

    if (s) {
        memcpy(addr, s + 8, *addrlen);
        *addrlen = *(int*)s;
    }
    return (int) ev->n;
}

int send(sock_t fd, const void* buf, int n, int ms) {
    IoEvent ev(fd, EV_write, buf, n);

    do {
        int r = WSASend(fd, &ev->buf, 1, &ev->n, 0, &ev->ol, 0);
        if (r == 0) {
            if (!can_skip_iocp_on_success) ev.wait();
        } else if (co::error() == WSA_IO_PENDING) {
            if (!ev.wait(ms)) return -1;
        } else {
            return -1;
        }

        do {
            if (ev->n == (DWORD) ev->buf.len) return n;
            if (ev->n == 0) return -1;
            ev->buf.buf += ev->n;
            ev->buf.len -= ev->n;
            memset(&ev->ol, 0, sizeof(ev->ol));
        } while (0);
    } while (true);
}

int sendto(sock_t fd, const void* buf, int n, const void* addr, int addrlen, int ms) {
    CHECK(scheduler()) << "must be called in coroutine..";
    IoEvent ev(fd, EV_write, buf, n);

    do {
        int r = WSASendTo(fd, &ev->buf, 1, &ev->n, 0, (const sockaddr*)addr, addrlen, &ev->ol, 0);
        if (r == 0) {
            if (!can_skip_iocp_on_success) ev.wait();
        } else if (co::error() == WSA_IO_PENDING) {
            if (!ev.wait(ms)) return -1;
        } else {
            return -1;
        }

        do {
            if (ev->n == (DWORD) ev->buf.len) return n;
            if (ev->n == 0) return -1;
            ev->buf.buf += ev->n;
            ev->buf.len -= ev->n;
            memset(&ev->ol, 0, sizeof(ev->ol));
        } while (0);
    } while (0);
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
                (LPTSTR)&s, 0, 0
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

bool can_skip_iocp_on_success() {
    static int protos[] = { IPPROTO_TCP, IPPROTO_UDP, 0 };
    fastring s;
    LPWSAPROTOCOL_INFOW proto_info = 0;
    DWORD buf_len = 0;
    int err = 0, ntry = 0;
    int r = WSCEnumProtocols(&protos[0], proto_info, &buf_len, &err);
    CHECK_EQ(r, SOCKET_ERROR);

    while (true) {
        CHECK_EQ(err, WSAENOBUFS);
        if (++ntry > 3) return false;

        s.reserve(buf_len);
        proto_info = (LPWSAPROTOCOL_INFOW) s.data();
        r = WSCEnumProtocols(&protos[0], proto_info, &buf_len, &err);
        if (r != SOCKET_ERROR) break;
    }

    for (int i = 0; i < r; ++i) {
        if (!(proto_info[i].dwServiceFlags1 & XP1_IFS_HANDLES)) return false;
    }
    return true;
}

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
    
    co::can_skip_iocp_on_success = xx::can_skip_iocp_on_success();
}

void wsa_cleanup() { WSACleanup(); }

} // xx
} // co

#endif // _WIN32
