#pragma once

#include "sock.h"

namespace co {

// io event type
enum _ev_t {
    ev_read = 1,
    ev_write = 2,
};

#ifndef _WIN32

// It is used to wait for an IO event on a socket.
class __coapi io_event {
  public:
    // @fd: a non-blocking socket
    // @ev: ev_read or ev_write
    io_event(sock_t fd, _ev_t ev)
        : _fd(fd), _ev(ev), _added(false) {
    }

    ~io_event();

    // Wait until the IO event is present, or timed out, or any error occured.
    // Return false on error or timedout, call co::error() to get the error code.
    bool wait(uint32 ms=(uint32)-1);

  private:
    sock_t _fd;
    _ev_t _ev;
    bool _added;
    DISALLOW_COPY_AND_ASSIGN(io_event);
};

#else
#ifdef _MSC_VER
#pragma warning (disable:4200)
#endif

namespace xx {

struct PerIoInfo {
    void* _1; // do not touch it
    void* _2; // do not touch it
    void* co;
    union {
        uint8 state;
        void* dummy;
    };

    WSAOVERLAPPED ol;
    DWORD n;      // bytes transfered
    DWORD flags;  // flags for WSARecv, WSARecvFrom
    WSABUF buf;   // buffer for WSARecv, WSARecvFrom, WSASend, WSASendTo
    DWORD mlen;   // memory length
    char s[];     // extra buffer allocated
};

// get PerIoInfo by a pointer to WSAOVERLAPPED
inline PerIoInfo* per_io_info(void* ol) {
    return (PerIoInfo*)((char*)ol - (size_t)&((PerIoInfo*)0)->ol);
}

} // xx

class __coapi io_event {
  public:
    static const int nb_tcp_recv = 1; // recv on non-blocking tcp socket
    static const int nb_tcp_send = 2; // send on non-blocking tcp socket

    // @fd: a TCP socket, it MUST be non-blocking and overlapped.
    // @ev: ev_read or ev_write.
    // No data will be transfered by IOCP in this case, as the buf length is 0.
    io_event(sock_t fd, _ev_t ev);

    // @fd: an overlapped socket, support both TCP and UDP, and it is not necessary 
    //      to be non-blocking.
    // @n:  extra bytes to be allocated with PerIoInfo.
    io_event(sock_t fd, int n=0);

    io_event(sock_t fd, _ev_t ev, const void* buf, int size, int n=0);

    ~io_event();

    xx::PerIoInfo* operator->() const {
        return _info;
    }

    // wait for the IO operation posted to IOCP to be done
    bool wait(uint32 ms=(uint32)-1);

  private:
    sock_t _fd;
    xx::PerIoInfo* _info;
    char* _to;
    int _nb_tcp; // for non-blocking tcp socket
    bool _timeout;
    DISALLOW_COPY_AND_ASSIGN(io_event);
};

#endif

typedef io_event IoEvent;

} // co
