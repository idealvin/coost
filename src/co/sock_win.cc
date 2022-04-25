#ifdef _WIN32

#include "scheduler.h"
#include <ws2spi.h>
#include <unordered_map>

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
    sock_t fd = CO_RAW_API(WSASocketW)(domain, type, protocol, 0, 0, WSA_FLAG_OVERLAPPED);
    if (fd != INVALID_SOCKET) {
        unsigned long mode = 1;
        CO_RAW_API(ioctlsocket)(fd, FIONBIO, &mode);
        if (type != SOCK_STREAM) set_skip_iocp_on_success(fd);
    }
    return fd;
}

int close(sock_t fd, int ms) {
    if (fd == (sock_t)-1) return CO_RAW_API(closesocket)(fd);
    co::get_sock_ctx(fd).del_event();
    if (ms > 0 && gSched) gSched->sleep(ms);
    return CO_RAW_API(closesocket)(fd);
}

int shutdown(sock_t fd, char c) {
    if (fd == (sock_t)-1) return CO_RAW_API(shutdown)(fd, SD_BOTH);
    auto& ctx = co::get_sock_ctx(fd);
    if (c == 'r') {
        ctx.del_ev_read();
        return CO_RAW_API(shutdown)(fd, SD_RECEIVE);
    } else if (c == 'w') {
        ctx.del_ev_write();
        return CO_RAW_API(shutdown)(fd, SD_SEND);
    } else {
        ctx.del_event();
        return CO_RAW_API(shutdown)(fd, SD_BOTH);
    }
}

int bind(sock_t fd, const void* addr, int addrlen) {
    return ::bind(fd, (const struct sockaddr*)addr, addrlen);
}

int listen(sock_t fd, int backlog) {
    return ::listen(fd, backlog);
}

inline int get_address_family(sock_t fd) {
    WSAPROTOCOL_INFOW info;
    int len = sizeof(info);
    int r = co::getsockopt(fd, SOL_SOCKET, SO_PROTOCOL_INFO, &info, &len);
    CHECK(r == 0 && info.iAddressFamily > 0) << "get address family failed, fd: " << fd;
    return info.iAddressFamily;
}

sock_t accept(sock_t fd, void* addr, int* addrlen) {
    CHECK(gSched) << "must be called in coroutine..";
    if (fd == (sock_t)-1) return CO_RAW_API(accept)(fd, (sockaddr*)addr, addrlen);

    // We have to figure out the address family of the listening socket here.
    int af;
    auto& ctx = co::get_sock_ctx(fd);
    if (ctx.has_event()) {
        af = ctx.get_address_family();
    } else {
        if (!gSched->add_io_event(fd, ev_read)) {
            ELOG << "add listening socket " << fd << " to IOCP failed..";
            return (sock_t)-1;
        }
        af = get_address_family(fd);
        ctx.set_address_family(af);
    }

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
        ELOG << "acceptex set SO_UPDATE_ACCEPT_CONTEXT error: " << co::strerror();
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
    CO_RAW_API(closesocket)(connfd);
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
            ELOG << "connectex bind local address failed..";
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
        ELOG << "connectex set SO_UPDATE_CONNECT_CONTEXT error: " << co::strerror();
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
    CHECK(gSched) << "must be called in coroutine..";
    IoEvent ev(fd, ev_read);

    do {
        int r = CO_RAW_API(recv)(fd, (char*)buf, n, 0);
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
    char* s = (char*)buf;
    int remain = n;
    IoEvent ev(fd, ev_read);

    do {
        int r = CO_RAW_API(recv)(fd, s, remain, 0);
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
    char* s = 0;
    const int N = (addr && addrlen) ? sizeof(SOCKADDR_STORAGE) + 8 : 0;
    IoEvent ev(fd, ev_read, buf, n, N);

    if (N > 0) {
        s = ev->s;
        *(int*)s = sizeof(SOCKADDR_STORAGE);
        r = CO_RAW_API(WSARecvFrom)(fd, &ev->buf, 1, &ev->n, &ev->flags, (sockaddr*)(s + 8), (int*)s, &ev->ol, 0);
    } else {
        r = CO_RAW_API(WSARecvFrom)(fd, &ev->buf, 1, &ev->n, &ev->flags, 0, 0, &ev->ol, 0);
    }

    if (r == 0) {
        if (!can_skip_iocp_on_success) ev.wait();
    } else if (co::error() == WSA_IO_PENDING) {
        if (!ev.wait(ms)) return -1;
    } else {
        return -1;
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
    int remain = n;
    IoEvent ev(fd, ev_write);

    do {
        int r = CO_RAW_API(send)(fd, s, remain, 0);
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
    IoEvent ev(fd, ev_write, buf, n);

    do {
        int r = CO_RAW_API(WSASendTo)(fd, &ev->buf, 1, &ev->n, 0, (const sockaddr*)addr, addrlen, &ev->ol, 0);
        if (r == 0) {
            if (!can_skip_iocp_on_success) ev.wait();
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
    } while (true);
}

void set_nonblock(sock_t fd) {
   unsigned long mode = 1;
   CO_RAW_API(ioctlsocket)(fd, FIONBIO, &mode);
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

const char* strerror(int e) {
    if (e == ETIMEDOUT || e == WSAETIMEDOUT) return "Timed out.";
    static auto ke = co::static_new<co::Error>();
    return ke->strerror(e);
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
