#ifdef _WIN32

#include "scheduler.h"
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
    static_assert(INVALID_SOCKET == (sock_t)-1, "");
    sock_t fd = __sys_api(WSASocketW)(domain, type, protocol, 0, 0, WSA_FLAG_OVERLAPPED);
    if (fd != (sock_t)-1) {
        unsigned long mode = 1;
        __sys_api(ioctlsocket)(fd, FIONBIO, &mode);
        if (type != SOCK_STREAM) set_skip_iocp_on_success(fd);
    } else {
        co::error() = WSAGetLastError();
    }
    return fd;
}

int close(sock_t fd, int ms) {
    if (fd == (sock_t)-1) return 0;

    co::get_sock_ctx(fd).del_event();
    if (ms > 0 && gSched) gSched->sleep(ms);

    int r = __sys_api(closesocket)(fd);
    if (r == 0) return 0;
    co::error() = WSAGetLastError();
    return r;
}

int shutdown(sock_t fd, char c) {
    if (fd == (sock_t)-1) return 0;

    int r;
    auto& ctx = co::get_sock_ctx(fd);
    switch (c) {
      case 'r':
        ctx.del_ev_read();
        r = __sys_api(shutdown)(fd, SD_RECEIVE);
        break;
      case 'w':
        ctx.del_ev_write();
        r = __sys_api(shutdown)(fd, SD_SEND);
        break;
      default:
        ctx.del_event();
        r = __sys_api(shutdown)(fd, SD_BOTH);
    }

    if (r == 0) return 0;
    co::error() = WSAGetLastError();
    return r;
}

int bind(sock_t fd, const void* addr, int addrlen) {
    int r = ::bind(fd, (const struct sockaddr*)addr, addrlen);
    if (r == 0) return 0;
    co::error() = WSAGetLastError();
    return r;
}

int listen(sock_t fd, int backlog) {
    int r = ::listen(fd, backlog);
    if (r == 0) return 0;
    co::error() = WSAGetLastError();
    return r;
}

inline int get_address_family(sock_t fd) {
    WSAPROTOCOL_INFOW info;
    int len = sizeof(info);
    int r = ::getsockopt(fd, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&info, &len);
    CHECK(r == 0 && info.iAddressFamily > 0) << "get address family failed, fd: " << fd;
    return info.iAddressFamily;
}

sock_t accept(sock_t fd, void* addr, int* addrlen) {
    CHECK(gSched) << "must be called in coroutine..";
    if (fd == (sock_t)-1) {
        co::error() = WSAENOTSOCK;
        return (sock_t)-1;
    }

    // We have to figure out the address family of the listening socket here.
    int af;
    auto& ctx = co::get_sock_ctx(fd);
    if (ctx.has_event()) {
        af = ctx.get_address_family();
    } else {
        gSched->add_io_event(fd, ev_read); // always return true on windows
        af = get_address_family(fd);
        ctx.set_address_family(af);
    }

    sock_t connfd = co::tcp_socket(af);
    if (connfd == (sock_t)-1) return connfd;

    const int N = sizeof(sockaddr_in6);
    IoEvent ev(fd, (N + 16) * 2);
    sockaddr *serv = 0, *peer = 0;
    int serv_len = N, peer_len = N, r, e;

    r = accept_ex(fd, connfd, ev->s, 0, N + 16, N + 16, 0, &ev->ol);
    if (r == FALSE) {
        e = WSAGetLastError();
        if (e != ERROR_IO_PENDING) goto err;
        ev.wait();
    }

    // https://docs.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-acceptex
    r = ::setsockopt(connfd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&fd, sizeof(fd));
    if (r != 0) {
        e = WSAGetLastError();
        ELOG << "acceptex set SO_UPDATE_ACCEPT_CONTEXT failed, sock: " << connfd;
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
    co::error() = e;
    __sys_api(closesocket)(connfd);
    return (sock_t)-1;
}

int connect(sock_t fd, const void* addr, int addrlen, int ms) {
    CHECK(gSched) << "must be called in coroutine..";

    // docs.microsoft.com/zh-cn/windows/win32/api/mswsock/nc-mswsock-lpfn_connectex
    // stackoverflow.com/questions/13598530/connectex-requires-the-socket-to-be-initially-bound-but-to-what
    // @fd must be an unconnected, previously bound socket
    do {
        SOCKADDR_STORAGE a = { 0 };
        a.ss_family = ((const sockaddr*)addr)->sa_family;
        if (co::bind(fd, &a, addrlen) != 0) {
            ELOG << "connectex bind local address failed, sock: " << fd;
            return -1;
        }
    } while (0);

    IoEvent ev(fd);
    int seconds, len = sizeof(int), r;

    r = connect_ex(fd, (const sockaddr*)addr, addrlen, 0, 0, 0, &ev->ol);
    if (r == FALSE) {
        if (WSAGetLastError() != ERROR_IO_PENDING) goto err;
        if (!ev.wait(ms)) return -1;
    }

    r = ::setsockopt(fd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0);
    if (r != 0) {
        ELOG << "connectex set SO_UPDATE_CONNECT_CONTEXT failed, sock: " << fd;
        goto err;
    }

    r = ::getsockopt(fd, SOL_SOCKET, SO_CONNECT_TIME, (char*)&seconds, &len);
    if (r == 0) {
        if (seconds < 0) {
            ELOG << "dragon here, connectex getsockopt(SO_CONNECT_TIME), seconds < 0, sock: " << fd;
            goto err;
        }
        set_skip_iocp_on_success(fd);
        return 0;
    } else {
        ELOG << "connectex getsockopt(SO_CONNECT_TIME) failed, sock: " << fd;
        goto err;
    }

  err:
    co::error() = WSAGetLastError();
    return -1;
}

int recv(sock_t fd, void* buf, int n, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    IoEvent ev(fd, ev_read);
    int r, e;

    do {
        r = __sys_api(recv)(fd, (char*)buf, n, 0);
        if (r != -1) return r;

        e = WSAGetLastError();
        if (e == WSAEWOULDBLOCK) {
            if (!ev.wait(ms)) return -1;
        } else {
            co::error() = e;
            return -1;
        }
    } while (true);
}

int recvn(sock_t fd, void* buf, int n, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    char* s = (char*)buf;
    int remain = n, r, e;
    IoEvent ev(fd, ev_read);

    do {
        r = __sys_api(recv)(fd, s, remain, 0);
        if (r == remain) return n;
        if (r == 0) return 0;

        if (r == -1) {
            e = WSAGetLastError();
            if (e == WSAEWOULDBLOCK) {
                if (!ev.wait(ms)) return -1;
            } else {
                co::error() = e;
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
    int r, e;
    char* s = 0;
    const int N = (addr && addrlen) ? sizeof(SOCKADDR_STORAGE) + 8 : 0;
    IoEvent ev(fd, ev_read, buf, n, N);

    if (N > 0) {
        s = ev->s;
        *(int*)s = sizeof(SOCKADDR_STORAGE);
        r = __sys_api(WSARecvFrom)(fd, &ev->buf, 1, &ev->n, &ev->flags, (sockaddr*)(s + 8), (int*)s, &ev->ol, 0);
    } else {
        r = __sys_api(WSARecvFrom)(fd, &ev->buf, 1, &ev->n, &ev->flags, 0, 0, &ev->ol, 0);
    }

    if (r == 0) {
        if (!can_skip_iocp_on_success) ev.wait();
    } else {
        e = WSAGetLastError();
        if (e == WSA_IO_PENDING) {
            if (!ev.wait(ms)) return -1;
        } else {
            co::error() = e;
            return -1;
        }
    }

    if (N > 0) {
        const int S = *(int*)s;
        if (S <= *addrlen) memcpy(addr, s + 8, S);
        *addrlen = S;
    }
    return (int)ev->n;
}

int send(sock_t fd, const void* buf, int n, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    const char* s = (const char*)buf;
    int remain = n, r, e;
    IoEvent ev(fd, ev_write);

    do {
        r = __sys_api(send)(fd, s, remain, 0);
        if (r == remain) return n;

        if (r == -1) {
            e = WSAGetLastError();
            if (e == WSAEWOULDBLOCK) {
                if (!ev.wait(ms)) return -1;
            } else {
                co::error() = e;
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
    int r, e;
    IoEvent ev(fd, ev_write, buf, n);

    do {
        r = __sys_api(WSASendTo)(fd, &ev->buf, 1, &ev->n, 0, (const sockaddr*)addr, addrlen, &ev->ol, 0);
        if (r == 0) {
            if (!can_skip_iocp_on_success) ev.wait();
        } else {
            e = WSAGetLastError();
            if (e == WSA_IO_PENDING) {
                if (!ev.wait(ms)) return -1;
            } else {
                co::error() = e;
                return -1;
            }
        }

        if (ev->n == (DWORD)ev->buf.len) return n;
        if (ev->n > 0 && ev->n < (DWORD)ev->buf.len) {
            ev->buf.buf += ev->n;
            ev->buf.len -= ev->n;
            memset(&ev->ol, 0, sizeof(ev->ol));
        } else {
            ELOG << "dragon here, sendto ev->n: " << ev->n << ", n: " << n << ", sock: " << fd;
            return -1;
        }
    } while (true);
}

void set_nonblock(sock_t fd) {
   unsigned long mode = 1;
   __sys_api(ioctlsocket)(fd, FIONBIO, &mode);
}

bool _can_skip_iocp_on_success() {
    int protos[] = { IPPROTO_TCP, IPPROTO_UDP, 0 };
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
        proto_info = (LPWSAPROTOCOL_INFOW)s.data();
        r = WSCEnumProtocols(&protos[0], proto_info, &buf_len, &err);
        if (r != SOCKET_ERROR) break;
    }

    for (int i = 0; i < r; ++i) {
        if (!(proto_info[i].dwServiceFlags1 & XP1_IFS_HANDLES)) return false;
    }
    return true;
}

void init_sock() {
    WSADATA x;
    WSAStartup(MAKEWORD(2, 2), &x);

    sock_t fd = ::socket(AF_INET, SOCK_STREAM, 0);
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
    can_skip_iocp_on_success = co::_can_skip_iocp_on_success();
}

void cleanup_sock() {
    WSACleanup();
}

} // co

#if defined(_MSC_VER) && defined(BUILDING_CO_SHARED)
extern "C" {

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    switch (reason) {
      case DLL_PROCESS_ATTACH:
        break;
      case DLL_THREAD_ATTACH:
        break;
      case DLL_THREAD_DETACH:
        break;
      case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

} // extern "C"
#endif

#endif // _WIN32
