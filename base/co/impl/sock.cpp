#ifdef _WIN32

#include "scheduler.h"
#include "io_event.h"
#include <ws2spi.h>

DEF_int32(co_max_recv_size, 1024 * 1024, "#1 max size for a single recv");
DEF_int32(co_max_send_size, 1024 * 1024, "#1 max size for a single send");

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

sock_t tcp_socket(int v) {
    return WSASocketW((v == 4 ? AF_INET : AF_INET6), SOCK_STREAM, 0, 0, 0, WSA_FLAG_OVERLAPPED);
}

sock_t udp_socket(int v) {
    sock_t fd = WSASocketW((v == 4 ? AF_INET : AF_INET6), SOCK_DGRAM, 0, 0, 0, WSA_FLAG_OVERLAPPED);
    if (fd != INVALID_SOCKET) set_skip_iocp_on_success(fd);
    return fd;
}

int close(sock_t fd, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    gSched->del_event(fd);
    if (ms > 0) gSched->sleep(ms);
    return ::closesocket(fd);
}

int shutdown(sock_t fd, char c) {
    CHECK(gSched) << "must be called in coroutine..";
    if (c == 'r') {
        gSched->del_event(fd, EV_read);
        return ::shutdown(fd, SD_RECEIVE);
    } else if (c == 'w') {
        gSched->del_event(fd, EV_write);
        return ::shutdown(fd, SD_SEND);
    } else {
        gSched->del_event(fd);
        return ::shutdown(fd, SD_BOTH);
    }
}

int bind(sock_t fd, const void* addr, int addrlen) {
    return ::bind(fd, (const struct sockaddr*) addr, (socklen_t) addrlen);
}

int listen(sock_t fd, int backlog) {
    return ::listen(fd, backlog);
}

// TODO: support ipv6?
// I'm not to do it myself. Someone help?  -- by Alvin at 2020.1.3
sock_t accept(sock_t fd, void* addr, int* addrlen) {
    CHECK(gSched) << "must be called in coroutine..";
    sock_t connfd = co::tcp_socket();
    if (connfd == INVALID_SOCKET) return connfd;

    const int sockaddr_len = sizeof(sockaddr_in);
    std::unique_ptr<PerIoInfo> info(
        new PerIoInfo(0, (sockaddr_len + 16) * 2, gSched->running())
    );

    sockaddr_in *serv = 0, *peer = 0;
    int serv_len = sizeof(*serv), peer_len = sizeof(*peer);
    IoEvent ev(fd, EV_read);

    int r = accept_ex(
        fd, connfd, info->s, 0,
        sockaddr_len + 16, sockaddr_len + 16,
        0, &info->ol
    );

    if (r == FALSE) {
        if (co::error() != ERROR_IO_PENDING) goto err;
        ev.wait();
    }

    // https://docs.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-acceptex
    r = setsockopt(
        connfd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
        (char*)&fd, sizeof(fd)
    );

    if (r != 0) {
        ELOG << "acceptex set SO_UPDATE_ACCEPT_CONTEXT failed..";
        goto err;
    }

    get_accept_ex_addrs(
        info->s, 0,
        sockaddr_len + 16, sockaddr_len + 16,
        (sockaddr**)&serv, &serv_len,
        (sockaddr**)&peer, &peer_len
    );

    if (addrlen) {
        if (*addrlen >= peer_len) memcpy(addr, peer, peer_len);
        *addrlen = peer_len;
    }

    set_skip_iocp_on_success(connfd);
    return connfd;

  err:
    ::closesocket(connfd);
    return -1;
}

int connect(sock_t fd, const void* addr, int addrlen, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    std::unique_ptr<PerIoInfo> info(
        new PerIoInfo(addr, addrlen, gSched->running())
    );

    // docs.microsoft.com/zh-cn/windows/win32/api/mswsock/nc-mswsock-lpfn_connectex
    // stackoverflow.com/questions/13598530/connectex-requires-the-socket-to-be-initially-bound-but-to-what
    // @fd must be an unconnected, previously bound socket
    do {
        struct sockaddr_in x;
        memset(&x, 0, sizeof(x));
        x.sin_family = AF_INET;
        if (co::bind(fd, &x, sizeof(x)) != 0) {
            ELOG << "connectex bind local addr failed..";
            return -1;
        }
    } while (0);

    IoEvent ev(fd, EV_write);

    int r = connect_ex(
        fd, (const sockaddr*)addr, addrlen,
        0, 0, 0, &info->ol
    );

    if (r == FALSE) {
        if (co::error() != ERROR_IO_PENDING) return -1;
        if (!ev.wait(ms, false)) return -1; // timeout
    }

    r = setsockopt(fd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0);
    if (r != 0) {
        ELOG << "connectex set SO_UPDATE_ACCEPT_CONTEXT failed..";
        return -1;
    }

    int seconds;
    int len = sizeof(int);
    r = getsockopt(fd, SOL_SOCKET, SO_CONNECT_TIME, (char*)&seconds, &len);
    if (r == NO_ERROR) {
        if (seconds == -1) return -1;
        set_skip_iocp_on_success(fd);
        return 0;
    } else {
        ELOG << "connectex getsockopt(SO_CONNECT_TIME) failed..";
        return -1;
    }
}

int recv(sock_t fd, void* buf, int n, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    std::unique_ptr<PerIoInfo> info(
        new PerIoInfo(!gSched->on_stack(buf) ? buf : 0, n, gSched->running())
    );

    IoEvent ev(fd, EV_read);
    int r = WSARecv(fd, &info->buf, 1, &info->n, &info->flags, &info->ol, 0);

    if (r == 0) {
        if (!can_skip_iocp_on_success) ev.wait();
    } else if (co::error() == WSA_IO_PENDING) {
        if (!ev.wait(ms)) { info->co = 0; return -1; }
    } else {
        return -1;
    }

    if (info->s && info->n > 0) memcpy(buf, info->s, info->n);
    return (int) info->n;
}

int _Recvn(sock_t fd, void* buf, int n, int ms) {
    std::unique_ptr<PerIoInfo> info(
        new PerIoInfo(!gSched->on_stack(buf) ? buf : 0, n, gSched->running())
    );

    IoEvent ev(fd, EV_read);

    do {
        int r = WSARecv(fd, &info->buf, 1, &info->n, &info->flags, &info->ol, 0);
        if (r == 0) {
            if (!can_skip_iocp_on_success) ev.wait();
        } else if (co::error() == WSA_IO_PENDING) {
            if (!ev.wait(ms)) { info->co = 0; return -1; }
        } else {
            return -1;
        }

        do {
            if (info->n == (DWORD) info->buf.len) {
                if (info->s) memcpy(buf, info->s, n);
                return n;
            }

            if (info->n == 0) return 0;

            info->move(info->n);
            info->resetol();
        } while (0);
    } while (true);
}

int recvn(sock_t fd, void* buf, int n, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    char* s = (char*) buf;
    int x = n;

    while (x > FLG_co_max_recv_size) {
        int r = _Recvn(fd, s, FLG_co_max_recv_size, ms);
        if (r != FLG_co_max_recv_size) return r;
        x -= FLG_co_max_recv_size;
        s += FLG_co_max_recv_size;
    }

    int r = _Recvn(fd, s, x, ms);
    return r != x ? r : n;
}

int recvfrom(sock_t fd, void* buf, int n, void* addr, int* addrlen, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    std::unique_ptr<PerIoInfo> info(
        new PerIoInfo(!gSched->on_stack(buf) ? buf : 0, n, gSched->running())
    );

    int r;
    char* s = 0;
    IoEvent ev(fd, EV_read);

    if (addr && addrlen) {
        s = (char*) malloc(*addrlen + 8);
        *(int*)s = *addrlen;
        r = WSARecvFrom(
            fd, &info->buf, 1, &info->n, &info->flags,
            (sockaddr*)(s + 8), (int*)s, &info->ol, 0
        );
    } else {
        r = WSARecvFrom(
            fd, &info->buf, 1, &info->n, &info->flags,
            0, 0, &info->ol, 0
        );
    }

    if (r == 0) {
        if (!can_skip_iocp_on_success) ev.wait();
    } else if (co::error() == WSA_IO_PENDING) {
        if (!ev.wait(ms)) { info->co = 0; return -1; }
    } else {
        return -1;
    }

    if (s) {
        memcpy(addr, s + 8, *addrlen);
        *addrlen = *(int*)s;
        free(s);
    }

    if (info->s && info->n > 0) memcpy(buf, info->s, info->n);
    return (int) info->n;
}

int _Send(sock_t fd, const void* buf, int n, int ms) {
    std::unique_ptr<PerIoInfo> info;
    if (!gSched->on_stack((void*)buf)) {
        info.reset(new PerIoInfo(buf, n, gSched->running()));
    } else {
        info.reset(new PerIoInfo(0, n, gSched->running()));
        memcpy(info->s, buf, n);
    }

    IoEvent ev(fd, EV_write);

    do {
        int r = WSASend(fd, &info->buf, 1, &info->n, 0, &info->ol, 0);

        if (r == 0) {
            if (!can_skip_iocp_on_success) ev.wait();
        } else if (co::error() == WSA_IO_PENDING) {
            if (!ev.wait(ms)) { info->co = 0; return -1; }
        } else {
            return -1;
        }

        do {
            if (info->n == (DWORD) info->buf.len) return n;
            if (info->n == 0) return -1;
            info->move(info->n);
            info->resetol();
        } while (0);
    } while (true);
}

int send(sock_t fd, const void* buf, int n, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    const char* s = (const char*) buf;
    int x = n;

    while (x > FLG_co_max_send_size) {
        int r = _Send(fd, s, FLG_co_max_send_size, ms);
        if (r != FLG_co_max_send_size) return r;
        x -= FLG_co_max_send_size;
        s += FLG_co_max_send_size;
    }

    int r = _Send(fd, s, x, ms);
    return r != x ? r : n;
}

int sendto(sock_t fd, const void* buf, int n, const void* addr, int addrlen, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    std::unique_ptr<PerIoInfo> info;
    if (!gSched->on_stack((void*)buf)) {
        info.reset(new PerIoInfo(buf, n, gSched->running()));
    } else {
        info.reset(new PerIoInfo(0, n, gSched->running()));
        memcpy(info->s, buf, n);
    }

    IoEvent ev(fd, EV_write);

    do {
        int r = WSASendTo(
            fd, &info->buf, 1, &info->n, 0,
            (const sockaddr*)addr, addrlen, &info->ol, 0
        );

        if (r == 0) {
            if (!can_skip_iocp_on_success) ev.wait();
        } else if (co::error() == WSA_IO_PENDING) {
            if (ms >= 0) {
                if (!ev.wait(ms)) { info->co = 0; return -1; }
            } else {
                ev.wait();
            }
        } else {
            return -1;
        }

        return (int) info->n;
    } while (0);
}

const char* strerror(int err) {
    static __thread std::unordered_map<int, const char*>* kErrStr = 0;
    if (!kErrStr) kErrStr = new std::unordered_map<int, const char*>();

    if (err == ETIMEDOUT) return "timedout";
    auto& e = (*kErrStr)[err];
    if (e) return e;

    char* s = 0;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        0, err,
        //MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
        (LPTSTR)&s, 0, 0
    );

    assert(s);
    e = _strdup(s);
    char* p = (char*) strchr(e, '\r');
    if (p) *p = '\0';
    return e;
}

bool _Can_skip_iocp_on_success() {
    static int protos[] = { IPPROTO_TCP, IPPROTO_UDP, 0 };

    fastring s;
    LPWSAPROTOCOL_INFOW proto_info = 0;
    DWORD buf_len = 0;
    int err = 0;

    int r = WSCEnumProtocols(
        (int*) &protos[0],
        proto_info, &buf_len, &err
    );
    CHECK_EQ(r, SOCKET_ERROR);

    int ntry = 0;
    bool done = false;

    while (true) {
        ++ntry;
        CHECK_EQ(err, WSAENOBUFS);
        CHECK_LE(ntry, 3);

        s.reserve(buf_len);
        proto_info = (LPWSAPROTOCOL_INFOW) s.data();

        r = WSCEnumProtocols(
            (int*) &protos[0],
            proto_info, &buf_len, &err
        );

        if (r != SOCKET_ERROR) break;
    }

    for (int i = 0; i < r; ++i) {
        if (!(proto_info[i].dwServiceFlags1 & XP1_IFS_HANDLES)) return false;
    }
    return true;
}

void _Wsa_startup() {
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

    can_skip_iocp_on_success = _Can_skip_iocp_on_success();
}

void _Wsa_cleanup() {
    WSACleanup();
}

} // co

#endif // _WIN32
