#ifdef _WIN32
#include "hook.h"

#ifdef _CO_DISABLE_HOOK
namespace co {
void init_hook() {}
void hook_sleep(bool) {}
} // co

#else
#include "sched.h"
#include "co/co.h"
#include "co/defer.h"
#include "co/flag.h"
#include "co/log.h"
#include "co/table.h"
#include "detours/detours.h"
#include <Mswsock.h>

DEF_bool(co_hook_log, false, ">>#1 enable log for hook if true");

#define HOOKLOG DLOG_IF(FLG_co_hook_log)

namespace co {
extern bool can_skip_iocp_on_success;

class HookCtx {
  public:
    HookCtx() : _v(0) {}
    HookCtx(const HookCtx& s) { _v = s._v; }
    ~HookCtx() = default;

    // set blocking or non-blocking mode for the socket.
    void set_non_blocking(int v) {
        _s.nb = !!v;
        if (_s.nb_mark) _s.nb_mark = 0;
    }

    bool is_non_blocking() const { return _s.nb; }
    bool is_blocking() const { return !this->is_non_blocking(); }

    void set_non_overlapped() { _s.no = 1; }
    bool is_non_overlapped() const { return _s.no; }
    bool is_overlapped() const { return !this->is_non_overlapped(); }

    // for blocking and non-overlapped sockets, we modify them to non-blocking, 
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
    static const uint8 f_skip_iocp = 4;
    static const uint8 f_non_sock_stream = 8;

    void set_shut_read() {
        if (atomic_or(&_s.flags, f_shut_read, mo_acq_rel) & f_shut_write) this->clear();
    }

    void set_shut_write() {
        if (atomic_or(&_s.flags, f_shut_write, mo_acq_rel) & f_shut_read) this->clear();
    }

    void set_skip_iocp()         { atomic_or(&_s.flags, f_skip_iocp, mo_acq_rel); }
    bool has_skip_iocp() const   { return _s.flags & f_skip_iocp; }
    void set_non_sock_stream()   { _s.flags |= f_non_sock_stream; }
    bool is_sock_stream() const  { return !(_s.flags & f_non_sock_stream); }

  private:
    union {
        uint64 _v;
        struct {
            uint8  nb;      // non-blocking
            uint8  no;      // non-overlapped
            uint8  nb_mark; // non-blocking mark
            uint8  flags;
            uint16 recv_timeout;
            uint16 send_timeout;
        } _s;
    };
};

class Hook {
  public:
    // _tb(14, 17) can hold 2^31=2G sockets
    Hook() : tb(14, 17), hook_sleep(true) {}
    ~Hook() = default;

    HookCtx* get_hook_ctx(sock_t s) {
        return s != INVALID_SOCKET ? &tb[(size_t)s] : NULL;
    }

    co::table<HookCtx> tb;
    bool hook_sleep;
};

} // co

static co::Hook* g_hook;

extern "C" {

_CO_DEF_SYS_API(Sleep);
_CO_DEF_SYS_API(socket);
_CO_DEF_SYS_API(WSASocketA);
_CO_DEF_SYS_API(WSASocketW);
_CO_DEF_SYS_API(closesocket);
_CO_DEF_SYS_API(shutdown);
_CO_DEF_SYS_API(setsockopt);
_CO_DEF_SYS_API(ioctlsocket);
_CO_DEF_SYS_API(WSAIoctl);
_CO_DEF_SYS_API(WSAAsyncSelect);
_CO_DEF_SYS_API(WSAEventSelect);
_CO_DEF_SYS_API(accept);
_CO_DEF_SYS_API(WSAAccept);
_CO_DEF_SYS_API(connect);
_CO_DEF_SYS_API(WSAConnect);
_CO_DEF_SYS_API(recv);
_CO_DEF_SYS_API(WSARecv);
_CO_DEF_SYS_API(recvfrom);
_CO_DEF_SYS_API(WSARecvFrom);
_CO_DEF_SYS_API(send);
_CO_DEF_SYS_API(WSASend);
_CO_DEF_SYS_API(sendto);
_CO_DEF_SYS_API(WSASendTo);
_CO_DEF_SYS_API(select);
_CO_DEF_SYS_API(WSAPoll);
_CO_DEF_SYS_API(WSAWaitForMultipleEvents);
_CO_DEF_SYS_API(GetQueuedCompletionStatus);
_CO_DEF_SYS_API(GetQueuedCompletionStatusEx);

WSARecvMsg_fp_t __sys_api(WSARecvMsg);
WSASendMsg_fp_t __sys_api(WSASendMsg);


inline void set_non_blocking(SOCKET s, u_long m) {
    __sys_api(ioctlsocket)(s, FIONBIO, &m);
}

inline void set_skip_iocp(sock_t s, co::HookCtx* ctx) {
    if (co::can_skip_iocp_on_success && !ctx->has_skip_iocp()) {
        ctx->set_skip_iocp();
        SetFileCompletionNotificationModes((HANDLE)s, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);
    }
}

void WINAPI hook_Sleep(
    DWORD a0
) {
    HOOKLOG << "hook_Sleep: " << a0;
    const auto sched = co::xx::gSched;
    if (!sched || !g_hook->hook_sleep) return __sys_api(Sleep)(a0);
    sched->sleep(a0);
}

SOCKET WINAPI hook_socket(
    int a0,
    int a1,
    int a2
) {
    SOCKET s = __sys_api(socket)(a0, a1, a2);
    auto ctx = g_hook->get_hook_ctx(s);
    if (ctx && a1 != SOCK_STREAM) ctx->set_non_sock_stream();
    HOOKLOG << "hook_socket, sock: " << (int)s;
    return s;
}

SOCKET WINAPI hook_WSASocketA(
    int                 a0,
    int                 a1,
    int                 a2,
    LPWSAPROTOCOL_INFOA a3,
    GROUP               a4,
    DWORD               a5
) {
    const bool ol = !!(a5 & WSA_FLAG_OVERLAPPED);
    SOCKET s = __sys_api(WSASocketA)(a0, a1, a2, a3, a4, a5);
    auto ctx = g_hook->get_hook_ctx(s);
    if (ctx) {
        if (!ol) ctx->set_non_overlapped();
        if (a1 != SOCK_STREAM) ctx->set_non_sock_stream();
    }
    HOOKLOG << "hook_WSASocketA, sock: " << (int)s << " overlapped: " << ol;
    return s;
}

SOCKET WINAPI hook_WSASocketW(
    int                 a0,
    int                 a1,
    int                 a2,
    LPWSAPROTOCOL_INFOW a3,
    GROUP               a4,
    DWORD               a5
) {
    const bool ol = !!(a5 & WSA_FLAG_OVERLAPPED);
    SOCKET s = __sys_api(WSASocketW)(a0, a1, a2, a3, a4, a5);
    auto ctx = g_hook->get_hook_ctx(s);
    if (ctx) {
        if (!ol) ctx->set_non_overlapped();
        if (a1 != SOCK_STREAM) ctx->set_non_sock_stream();
    }
    HOOKLOG << "hook_WSASocketW, sock: " << (int)s << " overlapped: " << ol;
    return s;
}

int WINAPI hook_closesocket(
    SOCKET s
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(s);
    if (ctx) {
        ctx->clear();
        r = co::close(s);
    } else {
        WSASetLastError(WSAENOTSOCK);
        r = SOCKET_ERROR;
    }
    HOOKLOG << "hook_closesocket, sock: " << (int)s << " r: " << r;
    return r;
}

int WINAPI hook_shutdown(
    SOCKET a0,
    int a1
) {
    if (a0 == INVALID_SOCKET) {
        WSASetLastError(WSAENOTSOCK);
        return SOCKET_ERROR;
    }

    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    switch (a1) {
      case SD_RECEIVE:
        ctx->set_shut_read();
        r = co::shutdown(a0, 'r');
        break;
      case SD_SEND:
        ctx->set_shut_write();
        r = co::shutdown(a0, 'w');
        break;
      case SD_BOTH:
        ctx->clear();
        r = co::shutdown(a0, 'b');
        break;
      default:
        WSASetLastError(WSAEINVAL);
        r = SOCKET_ERROR;
    }

    HOOKLOG << "hook_shutdown, sock: " << a0 << " a1: " << a1 << " r: " << r;
    return r;
}

int WINAPI hook_setsockopt(
    SOCKET      a0,
    int         a1,
    int         a2,
    const char* a3,
    int         a4
) {
    int r = __sys_api(setsockopt)(a0, a1, a2, a3, a4);
    if (r == 0 && a1 == SOL_SOCKET && (a2 == SO_RCVTIMEO || a2 == SO_SNDTIMEO)) {
        auto ctx = g_hook->get_hook_ctx(a0);
        const DWORD ms = *(DWORD*)a3;
        if (a2 == SO_RCVTIMEO) {
            HOOKLOG << "hook_setsockopt, sock: " << a0 << " recv timeout: " << ms;
            ctx->set_recv_timeout(ms);
        } else {
            HOOKLOG << "hook_setsockopt, sock: " << a0 << " send timeout: " << ms;
            ctx->set_send_timeout(ms);
        }
    }
    return r;
}

int WINAPI hook_ioctlsocket(
    SOCKET  a0,
    long    a1,
    u_long* a2 
) {
    int r = __sys_api(ioctlsocket)(a0, a1, a2);
    if (r == 0 && a1 == FIONBIO) {
        g_hook->get_hook_ctx(a0)->set_non_blocking((int)*a2);
        HOOKLOG << "hook_ioctlsocket, sock: " << a0 << " non_block: " << !!(*a2);
    }
    return r;
}

const int T = 16;

int WINAPI hook_WSAIoctl(
    SOCKET                             a0,
    DWORD                              a1,
    LPVOID                             a2,
    DWORD                              a3,
    LPVOID                             a4,
    DWORD                              a5,
    LPDWORD                            a6,
    LPWSAOVERLAPPED                    a7,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a8
) {
    if (a1 == FIONBIO) {
        int r = __sys_api(WSAIoctl)(a0, a1, a2, a3, a4, a5, a6, a7, a8);
        if (r == 0) {
            const int nb = (int)(*(u_long*)a2);
            g_hook->get_hook_ctx(a0)->set_non_blocking(nb);
            HOOKLOG << "hook_WSAIoctl FIONBIO, sock: " << a0 << " non_block: " << !!nb;
        }
        return r;
    }

    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking() || (ctx->is_overlapped() && a7) ||
        a1 == FIONREAD || a1 == SIOCATMARK) {
        return __sys_api(WSAIoctl)(a0, a1, a2, a3, a4, a5, a6, a7, a8);
    }

    int r, x = 1;
    set_non_blocking(a0, 1);
    while (true) {
        r = __sys_api(WSAIoctl)(a0, a1, a2, a3, a4, a5, a6, a7, a8);
        if (r == 0) break;
        if (WSAGetLastError() != WSAEWOULDBLOCK) break;
        sched->sleep(x);
        if (x < T) x <<= 1;
    }

    set_non_blocking(a0, 0);
    HOOKLOG << "hook_WSAIoctl, sock: " << a0 << " r: " << r;
    return r;
}

int WINAPI hook_WSAAsyncSelect(
    SOCKET a0,
    HWND   a1,
    u_int  a2,
    long   a3
) {
    int r = __sys_api(WSAAsyncSelect)(a0, a1, a2, a3);
    if (r == 0) g_hook->get_hook_ctx(a0)->set_non_blocking(1);
    HOOKLOG << "hook_WSAAsyncSelect, sock: " << a0 << " r: " << r;
    return r;
}

int WINAPI hook_WSAEventSelect(
    SOCKET   a0,
    WSAEVENT a1,
    long     a2
) {
    int r = __sys_api(WSAEventSelect)(a0, a1, a2);
    if (r == 0) g_hook->get_hook_ctx(a0)->set_non_blocking(1);
    HOOKLOG << "hook_WSAEventSelect, sock: " << a0 << " r: " << r;
    return r;
}

SOCKET WINAPI hook_accept(
    SOCKET a0,
    sockaddr* a1,
    int* a2
) {
    SOCKET r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(accept)(a0, a1, a2);
        goto end;
    }

    if (ctx->is_overlapped()) {
        r = co::accept(a0, a1, a2);
        goto end;
    }

    WLOG_FIRST_N(4) << "performance warning: accept on non-overlapped socket: " << a0;
    if (!ctx->has_nb_mark()) { set_non_blocking(a0, 1); ctx->set_nb_mark(); }
    {
        int x = 1;
        while (true) {
            r = __sys_api(accept)(a0, a1, a2);
            if (r != INVALID_SOCKET) goto end;
            if (WSAGetLastError() != WSAEWOULDBLOCK) goto end;
            sched->sleep(x);
            if (x < T) x <<= 1;
        }
    }

  end:
    HOOKLOG << "hook_accept, sock: " << a0 << " r: " << r;
    return r;
}

SOCKET WINAPI hook_WSAAccept(
    SOCKET a0,
    sockaddr* a1,
    LPINT a2,
    LPCONDITIONPROC a3,
    DWORD_PTR a4
) {
    SOCKET r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(WSAAccept)(a0, a1, a2, a3, a4);
        goto end;
    }

    if (ctx->is_overlapped() && !a3) {
        r = co::accept(a0, a1, a2);
        goto end;
    }

    if (a3) {
        WLOG_FIRST_N(4) << "performance warning: WSAAccept with condition, sock: " << a0;
    } else {
        WLOG_FIRST_N(4) << "performance warning: WSAAccept on non-overlapped socket: " << a0;
    }

    if (!ctx->has_nb_mark()) { set_non_blocking(a0, 1); ctx->set_nb_mark(); }
    {
        int x = 1;
        while (true) {
            r = __sys_api(WSAAccept)(a0, a1, a2, a3, a4);
            if (r != INVALID_SOCKET) goto end;
            if (WSAGetLastError() != WSAEWOULDBLOCK) goto end;
            sched->sleep(x);
            if (x < T) x <<= 1;
        }
    }

  end:
    HOOKLOG << "hook_WSAAccept, sock: " << a0 << " r: " << r;
    return r;
}

int WINAPI hook_connect(
    SOCKET a0,
    CONST sockaddr* a1,
    int a2
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(connect)(a0, a1, a2);
        goto end;
    }

    if (ctx->is_overlapped()) {
        r = co::connect(a0, a1, a2, ctx->send_timeout());
        goto end;
    }

    {
        WLOG_FIRST_N(4) << "performance warning: connect on non-overlapped socket: " << a0;
        set_non_blocking(a0, 1);
        defer(set_non_blocking(a0, 0));

        int sec = 0, len = sizeof(sec);
        uint32 t = ctx->send_timeout(), x = 1;
        r = __sys_api(connect)(a0, a1, a2);
        if (r == 0 || WSAGetLastError() != WSAEWOULDBLOCK) goto end;

        while (true) {
            r = co::getsockopt(a0, SOL_SOCKET, SO_CONNECT_TIME, &sec, &len);
            if (r != 0) goto end;    // r = SOCKET_ERROR
            if (sec >= 0) goto end;  // r = 0, connected
            if (t == 0) {
                WSASetLastError(WSAETIMEDOUT); // timeout
                r = -1; goto end;
            }
            sched->sleep(t > x ? x : t);
            if (t != (uint32)-1) t = (t > x ? t - x : 0);
            if (x < T) x <<= 1;
        }
    }

  end:
    HOOKLOG << "hook_connect, sock: " << a0 << " r: " << r;
    return r;
}

int WINAPI hook_WSAConnect(
    SOCKET a0,
    sockaddr* a1,
    int a2,
    LPWSABUF a3,
    LPWSABUF a4,
    LPQOS a5,
    LPQOS a6
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking()) {
        r = __sys_api(WSAConnect)(a0, a1, a2, a3, a4, a5, a6);
        goto end;
    }

    if (ctx->is_overlapped() && !a3 && !a4 && !a5) {
        r = co::connect(a0, a1, a2, ctx->send_timeout());
        goto end;
    }

    {
        if (ctx->is_non_overlapped()) {
            WLOG_FIRST_N(8) << "performance warning: WSAConnect on non-overlapped socket: " << a0;
        } else {
            WLOG_FIRST_N(8) << "performance warning: WSAConnect with extra connect data, sock: " << a0;
        }

        set_non_blocking(a0, 1);
        defer(set_non_blocking(a0, 0));

        int sec = 0, len = sizeof(sec);
        uint32 t = ctx->send_timeout(), x = 1;
        r = __sys_api(WSAConnect)(a0, a1, a2, a3, a4, a5, a6);
        if (r == 0 || WSAGetLastError() != WSAEWOULDBLOCK) goto end;

        while (true) {
            r = co::getsockopt(a0, SOL_SOCKET, SO_CONNECT_TIME, &sec, &len);
            if (r != 0) goto end;    // r = SOCKET_ERROR
            if (sec >= 0) goto end;  // r = 0, connected
            if (t == 0) {
                WSASetLastError(WSAETIMEDOUT); // timeout
                r = -1; goto end;
            }
            sched->sleep(t > x ? x : t);
            if (t != (uint32)-1) t = (t > x ? t - x : 0);
            if (x < T) x <<= 1;
        }
    }

  end:
    HOOKLOG << "hook_WSAConnect, sock: " << a0 << " r: " << r;
    return r;
}

// hook IO operations on blocking and non-overlapped sockets.
// _ctx:   hook context
//   _s:   socket
//   _t:   timeout
//  _ms:   max check timeval
//  _op:   IO operation
#define do_hard_hook(_ctx, _s, _t, _ms, _op) \
do { \
    if (!_ctx->has_nb_mark()) { set_non_blocking(_s, 1); _ctx->set_nb_mark(); } \
    uint32 t = _t, x = 1; \
    while (true) { \
        r = _op; \
        if (r >= 0 || WSAGetLastError() != WSAEWOULDBLOCK) goto end; \
        if (t == 0) { WSASetLastError(WSAETIMEDOUT); goto end; } \
        sched->sleep(t > x ? x : t); \
        if (t != (uint32)-1) t = (t > x ? t - x : 0); \
        if (x < _ms) x <<= 1; \
    } \
} while (0)

// As we use a shared stack for coroutines in the same thread, we MUST NOT pass 
// a buffer on the stack to IOCP.
static LPWSABUF check_wsabufs(LPWSABUF p, DWORD n, int do_memcpy) {
    bool on_stack = false;
    const auto sched = co::xx::gSched;
    for (DWORD i = 0; i < n; ++i) {
        if (sched->on_stack(p[i].buf)) { on_stack = true; break; }
    }
    if (!on_stack) return p;

    LPWSABUF x = (LPWSABUF) co::alloc(sizeof(WSABUF) * n);
    for (DWORD i = 0; i < n; ++i) {
        if (sched->on_stack(p[i].buf)) {
            x[i].buf = (char*) co::alloc(p[i].len);
            if (do_memcpy) memcpy(x[i].buf, p[i].buf, p[i].len);
        } else {
            x[i].buf = p[i].buf;
        }
        x[i].len = p[i].len;
    }
    return x;
}

static void clean_wsabufs(LPWSABUF x, LPWSABUF p, DWORD n, int do_memcpy) {
    for (DWORD i = 0; i < n; ++i) {
        if (x[i].buf != p[i].buf) {
            if (do_memcpy) memcpy(p[i].buf, x[i].buf, x[i].len);
            co::free(x[i].buf, x[i].len);
        }
    }
    co::free(x, sizeof(WSABUF) * n);
}

int WINAPI hook_recv(
    SOCKET a0,
    char* a1,
    int a2,
    int a3
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking() || a2 < 0) {
        r = __sys_api(recv)(a0, a1, a2, a3);
        goto end;
    }

    if (ctx->is_overlapped()) {
        co::io_event ev(a0, co::ev_read, a1, a2);
        ev->flags = a3;
        set_skip_iocp(a0, ctx);

        r = __sys_api(WSARecv)(a0, &ev->buf, 1, &ev->n, &ev->flags, &ev->ol, 0);
        if (r == 0) {
            if (!co::can_skip_iocp_on_success) ev.wait();
        } else if (WSAGetLastError() == WSA_IO_PENDING) {
            if (!ev.wait(ctx->recv_timeout())) goto end; // r = -1
        } else {
            goto end;
        }

        r = (int)ev->n;
        goto end;

    } else {
        WLOG_FIRST_N(8) << "performance warning: recv on non-overlapped socket: " << a0;
        do_hard_hook(ctx, a0, ctx->recv_timeout(), T, __sys_api(recv)(a0, a1, a2, a3));
    }

  end:
    HOOKLOG << "hook_recv, sock: " << a0 << " n: " << a2 << " r: " << r;
    return r;
}

int WINAPI hook_WSARecv(
    SOCKET a0,
    LPWSABUF a1,
    DWORD a2,
    LPDWORD a3,
    LPDWORD a4,
    LPWSAOVERLAPPED a5,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a6
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking() ||
        (ctx->is_overlapped() && (a5 || !a3))) {
        r = __sys_api(WSARecv)(a0, a1, a2, a3, a4, a5, a6);
        goto end;
    }

    if (ctx->is_overlapped()) {
        LPWSABUF x = check_wsabufs(a1, a2, 0);
        co::io_event ev(a0);
        if (a4) ev->flags = *a4;
        set_skip_iocp(a0, ctx);

        r = __sys_api(WSARecv)(a0, x, a2, &ev->n, &ev->flags, &ev->ol, a6);
        if (r == 0) {
            if (!co::can_skip_iocp_on_success) ev.wait();
        } else if (WSAGetLastError() == WSA_IO_PENDING) {
            if (ev.wait(ctx->recv_timeout())) r = 0;
        }

        if (r == 0) {
            *a3 = ev->n;
            if (a4) *a4 = ev->flags;
            if (x != a1) clean_wsabufs(x, a1, a2, 1);
            goto end;
        } else {
            *a3 = 0;
            if (x != a1) clean_wsabufs(x, a1, a2, 0);
            goto end;
        }

    } else {
        WLOG_FIRST_N(8) << "performance warning: WSARecv on non-overlapped socket: " << a0;
        do_hard_hook(ctx, a0, ctx->recv_timeout(), T, __sys_api(WSARecv)(a0, a1, a2, a3, a4, a5, a6));
    }

  end:
    HOOKLOG << "hook_WSARecv, sock: " << a0 << " r: " << r << " a3: " << ((r == 0 && a3) ? *a3 : 0);
    return r;
}

int WINAPI hook_recvfrom(
    SOCKET a0,
    char* a1,
    int a2,
    int a3,
    sockaddr* a4,
    int* a5
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking() || a2 < 0) {
        r = __sys_api(recvfrom)(a0, a1, a2, a3, a4, a5);
        goto end;
    }

    if (ctx->is_overlapped()) {
        char* s = 0;
        const int N = (a4 && a5) ? sizeof(SOCKADDR_STORAGE) + 8 : 0;
        co::io_event ev(a0, co::ev_read, a1, a2, N);
        ev->flags = a3;
        set_skip_iocp(a0, ctx);

        if (N > 0) {
            s = ev->s;
            *(int*)s = sizeof(SOCKADDR_STORAGE);
            r = __sys_api(WSARecvFrom)(a0, &ev->buf, 1, &ev->n, &ev->flags, (sockaddr*)(s + 8), (int*)s, &ev->ol, 0);
        } else {
            r = __sys_api(WSARecvFrom)(a0, &ev->buf, 1, &ev->n, &ev->flags, 0, 0, &ev->ol, 0);
        }

        if (r == 0) {
            if (!co::can_skip_iocp_on_success) ev.wait();
        } else if (WSAGetLastError() == WSA_IO_PENDING) {
            if (!ev.wait(ctx->recv_timeout())) goto end;
        } else {
            goto end;
        }

        if (N > 0) {
            const int x = *(int*)s;
            if (x <= *a5) memcpy(a4, s + 8, x);
            *a5 = x;
        }

        r = (int)ev->n;
        goto end;

    } else {
        WLOG_FIRST_N(8) << "performance warning: recvfrom on non-overlapped socket: " << a0;
        do_hard_hook(ctx, a0, ctx->recv_timeout(), T, __sys_api(recvfrom)(a0, a1, a2, a3, a4, a5));
    }

  end:
    HOOKLOG << "hook_recvfrom, sock: " << a0 << " n: " << a2 << " r: " << r;
    return r;
}

int WINAPI hook_WSARecvFrom(
    SOCKET a0,
    LPWSABUF a1,
    DWORD a2,
    LPDWORD a3,
    LPDWORD a4,
    sockaddr* a5,
    LPINT a6,
    LPWSAOVERLAPPED a7,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a8
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking() ||
        (ctx->is_overlapped() && (a7 || !a3))) {
        r = __sys_api(WSARecvFrom)(a0, a1, a2, a3, a4, a5, a6, a7, a8);
        goto end;
    }

    if (ctx->is_overlapped()) {
        LPWSABUF x = check_wsabufs(a1, a2, 0);
        const int N = (a5 && a6) ? sizeof(SOCKADDR_STORAGE) + 8 : 0;
        char* s = 0;
        co::io_event ev(a0, N);
        if (a4) ev->flags = *a4;
        set_skip_iocp(a0, ctx);

        if (N > 0) {
            s = ev->s;
            *(int*)s = sizeof(SOCKADDR_STORAGE);
            r = __sys_api(WSARecvFrom)(a0, x, a2, &ev->n, &ev->flags, (sockaddr*)(s + 8), (int*)s, &ev->ol, 0);
        } else {
            r = __sys_api(WSARecvFrom)(a0, x, a2, &ev->n, &ev->flags, 0, 0, &ev->ol, 0);
        }

        if (r == 0) {
            if (!co::can_skip_iocp_on_success) ev.wait();
        } else if (WSAGetLastError() == WSA_IO_PENDING) {
            if (ev.wait(ctx->recv_timeout())) r = 0;
        }

        if (r == 0) {
            *a3 = ev->n;
            if (a4) *a4 = ev->flags;
            if (N > 0) {
                const int peer_len = *(int*)s;
                if (peer_len <= *a6) memcpy(a5, s + 8, peer_len);
                *a6 = peer_len;
            }
            if (x != a1) clean_wsabufs(x, a1, a2, 1);
            goto end;
        } else {
            *a3 = 0;
            if (x != a1) clean_wsabufs(x, a1, a2, 0);
            goto end;
        }

    } else {
        WLOG_FIRST_N(8) << "performance warning: WSARecvFrom on non-overlapped socket: " << a0;
        do_hard_hook(ctx, a0, ctx->recv_timeout(), T, __sys_api(WSARecvFrom)(a0, a1, a2, a3, a4, a5, a6, a7, a8));
    }

  end:
    HOOKLOG << "hook_WSARecvFrom, sock: " << a0 << " r: " << r << " a3: " << ((r == 0 && a3) ? *a3 : 0);
    return r;
}

int WINAPI hook_send(
    SOCKET a0,
    CONST char* a1,
    int a2,
    int a3
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking() || a2 < 0) {
        r = __sys_api(send)(a0, a1, a2, a3);
        goto end;
    }

    if (ctx->is_overlapped()) {
        co::io_event ev(a0, co::ev_write, a1, a2);
        set_skip_iocp(a0, ctx);

        r = __sys_api(WSASend)(a0, &ev->buf, 1, &ev->n, a3, &ev->ol, 0);
        if (r == 0) {
            if (!co::can_skip_iocp_on_success) ev.wait();
        } else if (WSAGetLastError() == WSA_IO_PENDING) {
            if (!ev.wait(ctx->send_timeout())) goto end;
        } else {
            goto end;
        }

        r = (int)ev->n;
        goto end;

    } else {
        WLOG_FIRST_N(8) << "performance warning: send on non-overlapped socket: " << a0;
        do_hard_hook(ctx, a0, ctx->send_timeout(), T, __sys_api(send)(a0, a1, a2, a3));
    }

  end:
    HOOKLOG << "hook_send, sock: " << a0 << " n: " << a2 << " r: " << r;
    return r;
}

int WINAPI hook_WSASend(
    SOCKET a0,
    LPWSABUF a1,
    DWORD a2,
    LPDWORD a3,
    DWORD a4,
    LPWSAOVERLAPPED a5,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a6
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking() ||
        (ctx->is_overlapped() && (a5 || !a3))) {
        r = __sys_api(WSASend)(a0, a1, a2, a3, a4, a5, a6);
        goto end;
    }

    if (ctx->is_overlapped()) {
        LPWSABUF x = check_wsabufs(a1, a2, 1);
        co::io_event ev(a0);
        set_skip_iocp(a0, ctx);

        r = __sys_api(WSASend)(a0, x, a2, &ev->n, a4, &ev->ol, a6);
        if (r == 0) {
            if (!co::can_skip_iocp_on_success) ev.wait();
        } else if (WSAGetLastError() == WSA_IO_PENDING) {
            if (ev.wait(ctx->send_timeout())) r = 0;
        }

        *a3 = (r == 0 ? ev->n : 0);
        if (x != a1) clean_wsabufs(x, a1, a2, 0);
        goto end;

    } else {
        WLOG_FIRST_N(8) << "performance warning: WSASend on non-overlapped socket: " << a0;
        do_hard_hook(ctx, a0, ctx->send_timeout(), T, __sys_api(WSASend)(a0, a1, a2, a3, a4, a5, a6));
    }

  end:
    HOOKLOG << "hook_WSASend, sock: " << a0 << " r: " << r << " a3: " << ((r == 0 && a3) ? *a3 : 0);
    return r;
}

int WINAPI hook_sendto(
    SOCKET a0,
    CONST char* a1,
    int a2,
    int a3,
    CONST sockaddr* a4,
    int a5
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking() || a2 < 0) {
        r = __sys_api(sendto)(a0, a1, a2, a3, a4, a5);
        goto end;
    }

    if (ctx->is_overlapped()) {
        co::io_event ev(a0, co::ev_write, a1, a2);
        set_skip_iocp(a0, ctx);

        r = __sys_api(WSASendTo)(a0, &ev->buf, 1, &ev->n, a3, a4, a5, &ev->ol, 0);
        if (r == 0) {
            if (!co::can_skip_iocp_on_success) ev.wait();
        } else if (WSAGetLastError() == WSA_IO_PENDING) {
            if (!ev.wait(ctx->send_timeout())) goto end;
        } else {
            goto end;
        }

        r = (int)ev->n;
        goto end;

    } else {
        WLOG_FIRST_N(8) << "performance warning: sendto on non-overlapped socket: " << a0;
        do_hard_hook(ctx, a0, ctx->send_timeout(), T, __sys_api(sendto)(a0, a1, a2, a3, a4, a5));
    }

  end:
    HOOKLOG << "hook_sendto, sock: " << a0 << " n: " << a2 << " r: " << r;
    return r;
}

int WINAPI hook_WSASendTo(
    SOCKET a0,
    LPWSABUF a1,
    DWORD a2,
    LPDWORD a3,
    DWORD a4,
    CONST sockaddr* a5,
    int a6,
    LPWSAOVERLAPPED a7,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a8
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking() ||
        (ctx->is_overlapped() && (a7 || !a3))) {
        r = __sys_api(WSASendTo)(a0, a1, a2, a3, a4, a5, a6, a7, a8);
        goto end;
    }

    if (ctx->is_overlapped()) {
        LPWSABUF x = check_wsabufs(a1, a2, 1);
        co::io_event ev(a0);
        set_skip_iocp(a0, ctx);

        r = __sys_api(WSASendTo)(a0, x, a2, &ev->n, a4, a5, a6, &ev->ol, a8);
        if (r == 0) {
            if (!co::can_skip_iocp_on_success) ev.wait();
        } else if (WSAGetLastError() == WSA_IO_PENDING) {
            if (ev.wait(ctx->send_timeout())) r = 0;
        }

        *a3 = (r == 0 ? ev->n : 0);
        if (x != a1) clean_wsabufs(x, a1, a2, 0);
        goto end;

    } else {
        WLOG_FIRST_N(8) << "performance warning: WSASendTo on non-overlapped socket: " << a0;
        do_hard_hook(ctx, a0, ctx->send_timeout(), T, __sys_api(WSASendTo)(a0, a1, a2, a3, a4, a5, a6, a7, a8));
    }

  end:
    HOOKLOG << "hook_WSASendTo, sock: " << a0 << " r: " << r << " a3: " << ((r == 0 && a3) ? *a3 : 0);
    return r;
}

// c: 'r' for recv, 's' for send
static LPWSAMSG check_wsamsg(LPWSAMSG p, char c, int do_memcpy) {
    const auto sched = co::xx::gSched;
    bool mos = sched->on_stack(p);              // msg on stack
    bool aos = sched->on_stack(p->name);        // addr on stack
    bool cos = sched->on_stack(p->Control.buf); // control buf on stack
    auto buf = check_wsabufs(p->lpBuffers, p->dwBufferCount, do_memcpy);
    if (!mos && !cos && buf == p->lpBuffers && (!aos || c == 's')) return p;

    LPWSAMSG x = (LPWSAMSG) co::alloc(sizeof(*p));
    memcpy(x, p, sizeof(*p));

    if (c == 'r' && aos) {
        x->name = (LPSOCKADDR) co::alloc(sizeof(SOCKADDR_STORAGE));
        x->namelen = sizeof(SOCKADDR_STORAGE);
    }

    if (cos) {
        x->Control.buf = (char*) co::alloc(p->Control.len);
        if (do_memcpy) memcpy(x->Control.buf, p->Control.buf, p->Control.len);
    }

    if (buf != p->lpBuffers) x->lpBuffers = buf;
    return x;
}

static void clean_wsamsg(LPWSAMSG x, LPWSAMSG p, int do_memcpy) {
    if (x->name != p->name) {
        if (do_memcpy && x->namelen <= p->namelen) memcpy(p->name, x->name, x->namelen);
        co::free(x->name, sizeof(SOCKADDR_STORAGE));
    }
    p->namelen = x->namelen;

    if (x->Control.buf != p->Control.buf) {
        if (do_memcpy) memcpy(p->Control.buf, x->Control.buf, x->Control.len);
        co::free(x->Control.buf, p->Control.len);
    }
    p->Control.len = x->Control.len;

    if (x->lpBuffers != p->lpBuffers) {
        clean_wsabufs(x->lpBuffers, p->lpBuffers, p->dwBufferCount, do_memcpy);
    }

    co::free(x, sizeof(*p));
}

int WINAPI hook_WSARecvMsg(
    SOCKET                             a0,
    LPWSAMSG                           a1,
    LPDWORD                            a2,
    LPWSAOVERLAPPED                    a3,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a4
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking() ||
        (ctx->is_overlapped() && (a3 || !a2)) || !a1 ||
        (a1->name == NULL && a1->namelen != 0) ||
        (a1->Control.buf == NULL && a1->Control.len != 0)) {
        r = __sys_api(WSARecvMsg)(a0, a1, a2, a3, a4);
        goto end;
    }

    if (ctx->is_overlapped()) {
        auto x = check_wsamsg(a1, 'r', 0);
        co::io_event ev(a0);
        set_skip_iocp(a0, ctx);

        r = __sys_api(WSARecvMsg)(a0, x, &ev->n, &ev->ol, a4);
        if (r == 0) {
            if (!co::can_skip_iocp_on_success) ev.wait();
        } else if (WSAGetLastError() == WSA_IO_PENDING) {
            if (ev.wait(ctx->recv_timeout())) r = 0;
        }

        if (r == 0) {
            *a2 = ev->n;
            if (x != a1) clean_wsamsg(x, a1, 1);
            goto end;
        } else {
            *a2 = 0;
            if (x != a1) clean_wsamsg(x, a1, 0);
            goto end;
        }

    } else {
        WLOG_FIRST_N(8) << "performance warning: WSARecvMsg on non-overlapped socket: " << a0;
        do_hard_hook(ctx, a0, ctx->recv_timeout(), T, __sys_api(WSARecvMsg)(a0, a1, a2, a3, a4));
    }

  end:
    HOOKLOG << "hook_WSARecvMsg, sock: " << a0 << " r: " << r << " a2: " << ((r == 0 && a2) ? *a2 : 0);
    return r;
}

int WINAPI hook_WSASendMsg(
    SOCKET                             a0,
    LPWSAMSG                           a1,
    DWORD                              a2,
    LPDWORD                            a3,
    LPWSAOVERLAPPED                    a4,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a5
) {
    int r;
    auto ctx = g_hook->get_hook_ctx(a0);
    const auto sched = co::xx::gSched;
    if (!sched || !ctx || ctx->is_non_blocking() ||
        (ctx->is_overlapped() && (a4 || !a3)) || !a1 ||
        (a1->name == NULL && a1->namelen != 0) ||
        (a1->Control.buf == NULL && a1->Control.len != 0)) {
        r = __sys_api(WSASendMsg)(a0, a1, a2, a3, a4, a5);
        goto end;
    }

    if (ctx->is_overlapped()) {
        auto x = check_wsamsg(a1, 's', 1);
        co::io_event ev(a0);
        set_skip_iocp(a0, ctx);

        r = __sys_api(WSASendMsg)(a0, x, a2, &ev->n, &ev->ol, a5);
        if (r == 0) {
            if (!co::can_skip_iocp_on_success) ev.wait();
        } else if (WSAGetLastError() == WSA_IO_PENDING) {
            if (ev.wait(ctx->recv_timeout())) r = 0;
        }

        if (r == 0) {
            *a3 = ev->n;
            if (x != a1) clean_wsamsg(x, a1, 0);
            goto end;
        } else {
            *a3 = 0;
            if (x != a1) clean_wsamsg(x, a1, 0);
            goto end;
        }

    } else {
        WLOG_FIRST_N(8) << "performance warning: WSARecvMsg on non-overlapped socket: " << a0;
        do_hard_hook(ctx, a0, ctx->send_timeout(), T, __sys_api(WSASendMsg)(a0, a1, a2, a3, a4, a5));
    }

  end:
    HOOKLOG << "hook_WSASendMsg, sock: " << a0 << " r: " << r << " a3: " << ((r == 0 && a3) ? *a3 : 0);
    return r;
}

int WINAPI hook_select(
    int a0,
    fd_set* a1,
    fd_set* a2,
    fd_set* a3,
    const timeval* a4
) {
    const int64 max_ms = ((uint32)-1) >> 1;
    int r, ms = -1, t, x = 1;
    int64 sec, us;
    const auto sched = co::xx::gSched;

    if (a4) {
        sec = a4->tv_sec;
        us = a4->tv_usec;
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

    if (!sched || (!a1 && !a2 && !a3) || ms == 0 || sec < 0 || us < 0) {
        r = __sys_api(select)(a0, a1, a2, a3, a4);
        goto end;
    }

    {
        struct timeval tv = { 0, 0 };
        fd_set s[3];
        if (a1) s[0] = *a1;
        if (a2) s[1] = *a2;
        if (a3) s[2] = *a3;

        t = ms;
        while (true) {
            r = __sys_api(select)(a0, a1, a2, a3, &tv);
            if (r != 0 || t == 0) goto end;
            sched->sleep(t > x ? x : t);
            if (t != -1) t = (t > x ? t - x : 0);
            if (x < T) x <<= 1;
            if (a1) *a1 = s[0];
            if (a2) *a2 = s[1];
            if (a3) *a3 = s[2];
            tv.tv_sec = tv.tv_usec = 0;
        }
    }

  end:
    HOOKLOG << "hook_select, nfds: " << a0 << " ms: " << ms << " r: " << r;
    return r;
}

int WINAPI hook_WSAPoll(
    LPWSAPOLLFD a0,
    ULONG a1,
    INT a2
) {
    int r;
    const auto sched = co::xx::gSched;
    if (!sched || a2 == 0) {
        r = __sys_api(WSAPoll)(a0, a1, a2);
        goto end;
    }

    {
        uint32 t = a2 >= 0 ? a2 : -1, x = 1;
        while (true) {
            r = __sys_api(WSAPoll)(a0, a1, 0);
            if (r != 0 || t == 0) goto end;
            sched->sleep(t > x ? x : t);
            if (t != -1) t = (t > x ? t - x : 0);
            if (x < T) x <<= 1;
        }
    }

  end:
    HOOKLOG << "hook_WSAPoll, nfds: " << a1 <<  " ms: " << a2 << " r: " << r;
    return r;
}

DWORD WINAPI hook_WSAWaitForMultipleEvents(
    DWORD a0,
    CONST HANDLE* a1,
    BOOL a2,
    DWORD a3,
    BOOL a4
) {

    DWORD r, t = a3, x = 1;
    const auto sched = co::xx::gSched;
    if (!sched || a3 == 0) {
        r = __sys_api(WSAWaitForMultipleEvents)(a0, a1, a2, a3, a4);
        goto end;
    }

    while (true) {
        r = __sys_api(WSAWaitForMultipleEvents)(a0, a1, a2, 0, a4);
        if (r != WSA_WAIT_TIMEOUT || t == 0) goto end;
        sched->sleep(t > x ? x : t);
        if (t != WSA_INFINITE) t = (t > x ? t - x : 0);
        if (x < T) x <<= 1;
    }

  end:
    HOOKLOG << "hook_WSAWaitForMultipleEvents, n: " << a0 << " ms: " << a3 << " r: " << r;
    return r;
}

BOOL WINAPI hook_GetQueuedCompletionStatus(
    HANDLE        a0,
    LPDWORD       a1,
    PULONG_PTR    a2,
    LPOVERLAPPED* a3,
    DWORD         a4
) {
    BOOL r;
    DWORD t = a4, x = 1;
    const auto sched = co::xx::gSched;
    if (!sched) {
        r = __sys_api(GetQueuedCompletionStatus)(a0, a1, a2, a3, a4);
        goto end;
    }

    while (true) {
        r = __sys_api(GetQueuedCompletionStatus)(a0, a1, a2, a3, 0);
        if (r == TRUE || t == 0) goto end;
        sched->sleep(t > x ? x : t);
        if (t != INFINITE) t = (t > x ? t - x : 0);
        if (x < T) x <<= 1;
    }

  end:
    HOOKLOG << "hook_GetQueuedCompletionStatus, handle: " << a0 << " ms: " << a4 << " r: " << r;
    return r;
}

BOOL WINAPI hook_GetQueuedCompletionStatusEx(
    HANDLE             a0,
    LPOVERLAPPED_ENTRY a1,
    ULONG              a2,
    PULONG             a3,
    DWORD              a4,
    BOOL               a5
) {
    BOOL r;
    DWORD t = a4, x = 1;
    const auto sched = co::xx::gSched;
    if (!sched || a4 == 0) {
        r = __sys_api(GetQueuedCompletionStatusEx)(a0, a1, a2, a3, a4, a5);
        goto end;
    }

    while (true) {
        r = __sys_api(GetQueuedCompletionStatusEx)(a0, a1, a2, a3, 0, a5);
        if (r == TRUE || t == 0) goto end;
        sched->sleep(t > x ? x : t);
        if (t != INFINITE) t = (t > x ? t - x : 0);
        if (x < T) x <<= 1;
    }

  end:
    HOOKLOG << "hook_GetQueuedCompletionStatusEx, handle: " << a0 << " ms: " << a4 << " r: " << r;
    return r;
}

WSARecvMsg_fp_t get_WSARecvMsg_fp() {
    WSADATA x;
    WSAStartup(MAKEWORD(2, 2), &x);

    sock_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    CHECK_NE(fd, INVALID_SOCKET) << "create socket error: " << co::strerror();

    int r = 0;
    DWORD n = 0;
    GUID guid = WSAID_WSARECVMSG;
    WSARecvMsg_fp_t fp = NULL;
    r = WSAIoctl(
        fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guid, sizeof(guid),
        &fp, sizeof(fp),
        &n, 0, 0
    );
    CHECK_EQ(r, 0) << "get WSARecvMsg failed: " << co::strerror();
    CHECK(fp != NULL) << "pointer to WSARecvMsg is NULL..";

    ::closesocket(fd);
    return fp;
}

WSASendMsg_fp_t get_WSASendMsg_fp() {
    WSADATA x;
    WSAStartup(MAKEWORD(2, 2), &x);

    sock_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    CHECK_NE(fd, INVALID_SOCKET) << "create socket error: " << co::strerror();

    int r = 0;
    DWORD n = 0;
    GUID guid = WSAID_WSASENDMSG;
    WSASendMsg_fp_t fp = NULL;
    r = WSAIoctl(
        fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guid, sizeof(guid),
        &fp, sizeof(fp),
        &n, 0, 0
    );
    CHECK_EQ(r, 0) << "get WSASendMsg failed: " << co::strerror();
    CHECK(fp != NULL) << "pointer to WSASendMsg is NULL..";

    ::closesocket(fd);
    return fp;
}

} // extern "C"

namespace co {

inline void detour_attach(PVOID* ppbReal, PVOID pbMine, PCHAR psz) {
    LONG l = DetourAttach(ppbReal, pbMine);
    CHECK_EQ(l, 0) << "detour attach failed: " << psz;
}

inline void detour_detach(PVOID* ppbReal, PVOID pbMine, PCHAR psz) {
    LONG l = DetourDetach(ppbReal, pbMine);
    CHECK_EQ(l, 0) << "detour detach failed: " << psz;
}

#define attach_hook(x)  detour_attach(&(PVOID&)__sys_api(x), (PVOID)hook_##x, #x)
#define detach_hook(x)  detour_detach(&(PVOID&)__sys_api(x), (PVOID)hook_##x, #x)

// it will be called at initialization of coroutine schedulers
void init_hook() {
    __sys_api(WSARecvMsg) = get_WSARecvMsg_fp();
    __sys_api(WSASendMsg) = get_WSASendMsg_fp();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    attach_hook(Sleep);
    attach_hook(socket);
    attach_hook(WSASocketA);
    attach_hook(WSASocketW);
    attach_hook(closesocket);
    attach_hook(shutdown);
    attach_hook(setsockopt);
    attach_hook(ioctlsocket);
    attach_hook(WSAIoctl);
    attach_hook(WSAAsyncSelect);
    attach_hook(WSAEventSelect);
    attach_hook(accept);
    attach_hook(WSAAccept);
    attach_hook(connect);
    attach_hook(WSAConnect);
    attach_hook(recv);
    attach_hook(WSARecv);
    attach_hook(recvfrom);
    attach_hook(WSARecvFrom);
    attach_hook(send);
    attach_hook(WSASend);
    attach_hook(sendto);
    attach_hook(WSASendTo);
    attach_hook(WSARecvMsg);
    attach_hook(WSASendMsg);
    attach_hook(select);
    attach_hook(WSAPoll);
    attach_hook(WSAWaitForMultipleEvents);
    attach_hook(GetQueuedCompletionStatus);
    attach_hook(GetQueuedCompletionStatusEx);
    DetourTransactionCommit();
}

#if 0
void cleanup_hook() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    detach_hook(Sleep);
    detach_hook(socket);
    detach_hook(WSASocketA);
    detach_hook(WSASocketW);
    detach_hook(closesocket);
    detach_hook(shutdown);
    detach_hook(setsockopt);
    detach_hook(ioctlsocket);
    detach_hook(WSAIoctl);
    detach_hook(WSAAsyncSelect);
    detach_hook(WSAEventSelect);
    detach_hook(accept);
    detach_hook(WSAAccept);
    detach_hook(connect);
    detach_hook(WSAConnect);
    detach_hook(recv);
    detach_hook(WSARecv);
    detach_hook(recvfrom);
    detach_hook(WSARecvFrom);
    detach_hook(send);
    detach_hook(WSASend);
    detach_hook(sendto);
    detach_hook(WSASendTo);
    detach_hook(WSARecvMsg);
    detach_hook(WSASendMsg);
    detach_hook(select);
    detach_hook(WSAPoll);
    detach_hook(WSAWaitForMultipleEvents);
    detach_hook(GetQueuedCompletionStatus);
    detach_hook(GetQueuedCompletionStatusEx);
    DetourTransactionCommit();
}
#endif

void hook_sleep(bool x) {
    atomic_store(&g_hook->hook_sleep, x, mo_relaxed);
}

} // co

static int g_nifty_counter;
HookInitializer::HookInitializer() {
    if (g_nifty_counter++ == 0) {
        g_hook = co::_make_static<co::Hook>();
    }
}

HookInitializer::~HookInitializer() {}

#undef attach_hook
#undef detach_hook
#undef do_hard_hook
#undef HOOKLOG

#endif // _CO_DISABLE_HOOK
#endif // #ifdef _WIN32
