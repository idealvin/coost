#pragma once

#include "sock.h"

namespace co {

enum io_event_t {
    ev_read = 1,
    ev_write = 2,
};

#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning (disable:4200)
#endif

/**
 * IoEvent is for waiting an IO event on a socket 
 *   - It MUST be used in a coroutine. 
 *   - This is different from IoEvent for linux & mac. Windows users should be 
 *     careful. See details in the comments below.
 */
class __coapi IoEvent {
  public:
    struct PerIoInfo {
        // co::waitx_t:  {co, state}
        void* co;      // Coroutine
        union {
            uint8 state; // co_state_t
            void* dummy;
        };

        WSAOVERLAPPED ol;
        DWORD n;      // bytes transfered
        DWORD flags;  // flags for WSARecv, WSARecvFrom
        WSABUF buf;   // buffer for WSARecv, WSARecvFrom, WSASend, WSASendTo
        DWORD mlen;   // memory length
        char s[];     // extra buffer allocated
    };
    
    static const int nb_tcp_recv = 1; // recv on non-blocking tcp socket
    static const int nb_tcp_send = 2; // send on non-blocking tcp socket

    /**
     * the constructor with IO event on a non-blocking TCP socket 
     *   - This case works for non-blocking TCP sockets only.
     * 
     * @param fd  a TCP socket, it MUST be non-blocking and overlapped.
     * @param ev  the IO event, either ev_read or ev_write.
     */
    IoEvent(sock_t fd, io_event_t ev);

    /**
     * the constructor without any IO event 
     *   - This case works for any overlapped sockets.
     *   - If n > 0, an extra n-byte buffer will be allocated with the PerIoInfo, 
     *     and users can use IoEvent->s to access the extra buffer. 
     * 
     * @param fd  an overlapped socket, support both TCP and UDP, and it is not 
     *            necessary to be non-blocking.
     * @param n   extra bytes to be allocated with PerIoInfo.
     */
    IoEvent(sock_t fd, int n=0);

    IoEvent(sock_t fd, io_event_t ev, const void* buf, int size, int n=0);

    ~IoEvent();

    PerIoInfo* operator->() const {
        return _info;
    }

    /**
     * wait for an IO event on a socket 
     *   - If the first constructor was used, no data will be transfered by IOCP. 
     *     wait() blocks until the socket is readable or writable, or timeout, or 
     *     any error occured.
     *   - If the second constructor was used, users MUST firstly create an IoEvent, 
     *     and call WSARecv, WSASend or other APIs to post an IO operation to IOCP, 
     *     and then call wait() to wait for the operation to be done.
     * 
     * @param ms  timeout in milliseconds, default: -1, never timeout. 
     * 
     * @return    true if an IO event is present or the IO operation was done, 
     *            or false on timeout or error, call co::error() to get the error code.
     */
    bool wait(uint32 ms=(uint32)-1);

  private:
    sock_t _fd;
    PerIoInfo* _info;
    char* _to;
    int _nb_tcp; // for non-blocking tcp socket
    bool _timeout;
    DISALLOW_COPY_AND_ASSIGN(IoEvent);
};

#else
/**
 * IoEvent is for waiting an IO event on a socket 
 *   - It MUST be used in a coroutine. 
 *   - The socket MUST be non-blocking on Linux & Mac. 
 */
class IoEvent {
  public:
    /**
     * the constructor 
     * 
     * @param fd  a non-blocking socket.
     * @param ev  the IO event, either ev_read or ev_write.
     */
    IoEvent(sock_t fd, io_event_t ev)
        : _fd(fd), _ev(ev), _has_ev(false) {
    }

    // remove IO event from epoll or kqueue.
    ~IoEvent();

    /**
     * wait for an IO event on a socket 
     *   - wait() blocks until the IO event is present, or timeout, or any error occured. 
     * 
     * @param ms  timeout in milliseconds, default: -1, never timeout. 
     * 
     * @return    true if an IO event is present, or false on timeout or error,
     *            call co::error() to get the error code.
     */
    bool wait(uint32 ms=(uint32)-1);

  private:
    sock_t _fd;
    io_event_t _ev;
    bool _has_ev;
    DISALLOW_COPY_AND_ASSIGN(IoEvent);
};
#endif

} // co
