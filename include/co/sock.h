#pragma once

#include "def.h"
#include "error.h"
#include "string.h"
#include "byte_order.h"

#ifdef _WIN32
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

#if __arch64
typedef uint64 sock_t;
#else
typedef uint32 sock_t;
#endif

#else
typedef int sock_t;
#endif

namespace co {

struct alignas(8) sockaddr {
    sockaddr();
    sockaddr(const char* host, uint16 port);
    ~sockaddr() = default;

    bool valid() const;
    int af() const; // address family

    uint32 _p[10];
};

co::string& operator<<(co::string& s, const co::sockaddr& addr);

enum ev_t {
    ev_read = 1,
    ev_write = 2,
};

#ifndef _WIN32

// used to wait for I/O event on a socket
struct io_event {
    io_event(sock_t fd, ev_t ev)
        : _fd(fd), _ev(ev), _added(false) {
    }

    ~io_event();

    io_event(const io_event&) = delete;
    io_event(io_event&&) = delete;
    void operator=(const io_event&) = delete;
    void operator=(io_event&&) = delete;

    bool wait(uint32 ms=(uint32)-1);

    sock_t _fd;
    ev_t _ev;
    bool _added;
};

#else /* _WIN32 */

namespace xx {
struct SockInit {
    SockInit();
    ~SockInit();
};

static SockInit g_sock_init;
} // xx

// used to wait for I/O event on a socket
struct io_event {
    // @fd: a non-blocking and overlapped TCP socket
    // for tcp (recv, send)
    io_event(sock_t fd, ev_t ev);

    // @fd: an overlapped socket
    // @n:  extra bytes to be allocated with PerIoInfo
    // for connect, accept
    io_event(sock_t fd, int n=0);

    // for udp (recvfrom, sendto)
    io_event(sock_t fd, ev_t ev, const void* buf, int size, int n=0);

    ~io_event();

    io_event(const io_event&) = delete;
    io_event(io_event&&) = delete;
    void operator=(const io_event&) = delete;
    void operator=(io_event&&) = delete;

    bool wait(uint32 ms=(uint32)-1);

    sock_t _fd;
    ev_t _ev;
    int _type;
    void* _per_io;
    void* _copy_to;
};

#endif

sock_t tcp_socket(int af=0);

sock_t udp_socket(int af=0);

// create a TCP socket, bind the address and listen on it
sock_t tcp_server_socket(const char* host, uint16 port, int backlog=4096);

int close(sock_t fd, int ms=0);

int bind(sock_t fd, const sockaddr& addr);

int listen(sock_t fd, int backlog=4096);

sock_t accept(sock_t fd, sockaddr* addr=nullptr);

int connect(sock_t fd, const sockaddr& addr, int ms=-1);

int recv(sock_t fd, void* buf, int n, int ms=-1);

// recv n bytes
int recvn(sock_t fd, void* buf, int n, int ms=-1);

int recvfrom(sock_t fd, void* buf, int n, sockaddr& addr, int ms=-1);

int send(sock_t fd, const void* buf, int n, int ms=-1);

int sendto(sock_t fd, const void* buf, int n, const sockaddr& addr, int ms=-1);

void set_reuseaddr(sock_t fd);
void set_tcp_nodelay(sock_t fd);
void set_tcp_keepalive(sock_t fd);

// set buffer size, MUST be called before the socket is connected
void set_send_buffer_size(sock_t fd, int n);
void set_recv_buffer_size(sock_t fd, int n);

void set_nonblock(sock_t fd);

#ifndef _WIN32
void set_cloexec(sock_t fd);
#endif

// used in TCP server to reset a connection 
int reset_tcp_socket(sock_t fd, int ms=0);

// peer address of a connected socket
sockaddr peeraddr(sock_t fd);

} // co
