#pragma once

#include "co/def.h"
#include "co/byte_order.h"
#include "co/fastring.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h> // for inet_ntop...
#include <MSWSock.h>
#pragma comment(lib, "Ws2_32.lib")

typedef SOCKET sock_t;

#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>  // basic socket api, struct linger
#include <netinet/in.h>  // for struct sockaddr_in
#include <netinet/tcp.h> // for TCP_NODELAY...
#include <arpa/inet.h>   // for inet_ntop...
#include <netdb.h>       // getaddrinfo, gethostby...

typedef int sock_t;
#endif

namespace co {

/** 
 * create a socket suitable for coroutine programing
 * 
 * @param domain  address family, AF_INET, AF_INET6, etc.
 * @param type    socket type, SOCK_STREAM, SOCK_DGRAM, etc.
 * @param proto   protocol, IPPROTO_TCP, IPPROTO_UDP, etc.
 * 
 * @return        a non-blocking socket on Linux & Mac, an overlapped socket on windows, 
 *                or -1 on error.
 */
sock_t socket(int domain, int type, int proto);

/**
 * create a TCP socket suitable for coroutine programing
 * 
 * @param domain  address family, AF_INET, AF_INET6, etc.
 *                default: AF_INET
 * 
 * @return        a non-blocking socket on Linux & Mac, an overlapped socket on windows, 
 *                or -1 on error.
 */
inline sock_t tcp_socket(int domain=AF_INET) {
    return co::socket(domain, SOCK_STREAM, IPPROTO_TCP);
}

/**
 * create a UDP socket suitable for coroutine programing
 * 
 * @param domain  address family, AF_INET, AF_INET6, etc.
 *                default: AF_INET
 * 
 * @return        a non-blocking socket on Linux & Mac, an overlapped socket on windows, 
 *                or -1 on error.
 */
inline sock_t udp_socket(int domain=AF_INET) {
    return co::socket(domain, SOCK_DGRAM, IPPROTO_UDP);
}

/**
 * close a socket 
 *   - A socket SHOULD be closed in the coroutine where you call connect, recv, 
 *     recvn, recvfrom, send or sendto, in other words, close the socket in the 
 *     coroutine where you send or recieve network data. 
 *   - EINTR has been handled internally. The user need not consider about it.
 *     
 * @param fd  the socket, which is non-blocking on Linux & Mac, overlapped on windows
 * @param ms  if ms > 0, the socket will be closed ms milliseconds later. 
 *            default: 0.
 * 
 * @return    0 on success, -1 on error
 */
int close(sock_t fd, int ms=0);

/**
 * shutdown a socket 
 *   - Like the close, shutdown SHOULD be called in the coroutine where you send 
 *     or recieve network data.
 * 
 * @param fd  the socket, which is non-blocking on Linux & Mac, overlapped on windows
 * @param c   'r' for SHUT_RD, 'w' for SHUT_WR, 'b' for SHUT_RDWR.
 *            default: 'b'.
 * 
 * @return    0 on success, -1 on error
 */
int shutdown(sock_t fd, char c='b');

/**
 * bind an address to a socket
 * 
 * @param fd       the socket, which is non-blocking on Linux & Mac, overlapped on windows
 * @param addr     a pointer to struct sockaddr, sockaddr_in or sockaddr_in6
 * @param addrlen  size of the structure pointed to by addr
 * 
 * @return         0 on success, -1 on error
 */
int bind(sock_t fd, const void* addr, int addrlen);

/**
 * listen on a socket
 * 
 * @param fd       the socket, which is non-blocking on Linux & Mac, overlapped on windows
 * @param backlog  maximum length of the queue for pending connections.
 *                 default: 1024
 * 
 * @return         0 on success, -1 on error
 */
int listen(sock_t fd, int backlog=1024);

/**
 * accept a connection on a socket 
 *   - It MUST be called in a coroutine. 
 *   - accept() blocks until a connection is present, it behaves like that the 
 *     socket is blocking, which though is actually non-blocking or overlapped. 
 * 
 * @param fd       the socket, which is non-blocking on Linux & Mac, overlapped on windows
 * @param addr     a pointer to struct sockaddr, sockaddr_in or sockaddr_in6
 * @param addrlen  a value-result argument: the caller must initialize it with 
 *                 the size of the structure pointed to by addr; on return it 
 *                 will contain the actual size of the peer address.
 * 
 * @return         a non-blocking socket on Linux & Mac, an overlapped socket on windows,
 *                 or -1 on error.
 */
sock_t accept(sock_t fd, void* addr, int* addrlen);

/**
 * connect until the connection is done or timeout, or any error occured 
 *   - It MUST be called in a coroutine. 
 *   - It behaves like that the socket is blocking, which though is actually 
 *     non-blocking or overlapped. 
 * 
 * @param fd       the socket, which is non-blocking on Linux & Mac, overlapped on windows
 * @param addr     a pointer to struct sockaddr, sockaddr_in or sockaddr_in6
 * @param addrlen  size of the structure pointed to by addr
 * @param ms       timeout in milliseconds, if ms < 0, it will never time out. 
 *                 default: -1 
 */
int connect(sock_t fd, const void* addr, int addrlen, int ms=-1);

// recv until 0 or more bytes are received or timeout in @ms, or any error occured
int recv(sock_t fd, void* buf, int n, int ms=-1);

// recv until all @n bytes are done or timeout in @ms, or any error occured
int recvn(sock_t fd, void* buf, int n, int ms=-1);

int recvfrom(sock_t fd, void* buf, int n, void* addr, int* addrlen, int ms=-1);

// send until all @n bytes are done or timeout in @ms, or any error occured
int send(sock_t fd, const void* buf, int n, int ms=-1);

// for udp, max(n) == 65507
int sendto(sock_t fd, const void* buf, int n, const void* addr, int addrlen, int ms=-1);

#ifdef _WIN32
inline int getsockopt(sock_t fd, int lv, int opt, void* optval, int* optlen) {
    return ::getsockopt(fd, lv, opt, (char*)optval, optlen);
}

inline int setsockopt(sock_t fd, int lv, int opt, const void* optval, int optlen) {
    return ::setsockopt(fd, lv, opt, (const char*)optval, optlen);
}
#else
inline int getsockopt(sock_t fd, int lv, int opt, void* optval, int* optlen) {
    return ::getsockopt(fd, lv, opt, optval, (socklen_t*)optlen);
}

inline int setsockopt(sock_t fd, int lv, int opt, const void* optval, int optlen) {
    return ::setsockopt(fd, lv, opt, optval, (socklen_t)optlen);
}
#endif

inline void set_reuseaddr(sock_t fd) {
    int v = 1;
    co::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
}

// !! send/recv buffer size must be set before the socket is connected.
inline void set_send_buffer_size(sock_t fd, int n) {
    co::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n));
}

inline void set_recv_buffer_size(sock_t fd, int n) {
    co::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));
}

inline void set_tcp_nodelay(sock_t fd) {
    int v = 1;
    co::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v));
}

inline void set_tcp_keepalive(sock_t fd) {
    int v = 1;
    co::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &v, sizeof(v));
}

// reset tcp connection @ms milliseconds later
inline void reset_tcp_socket(sock_t fd, int ms=0) {
    struct linger v = { 1, 0 };
    co::setsockopt(fd, SOL_SOCKET, SO_LINGER, &v, sizeof(v));
    co::close(fd, ms);
}

#ifndef _WIN32
inline void set_nonblock(sock_t fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

inline void set_cloexec(sock_t fd) {
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
}
#endif

// fill in ipv4 addr with ip & port
inline bool init_ip_addr(struct sockaddr_in* addr, const char* ip, int port) {
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = hton16((uint16) port);
    return inet_pton(AF_INET, ip, &addr->sin_addr) == 1;
}

// fill in ipv6 addr with ip & port
inline bool init_ip_addr(struct sockaddr_in6* addr, const char* ip, int port) {
    memset(addr, 0, sizeof(*addr));
    addr->sin6_family = AF_INET6;
    addr->sin6_port = hton16((uint16) port);
    return inet_pton(AF_INET6, ip, &addr->sin6_addr) == 1;
}

// get ip string from ipv4 addr
inline fastring ip_str(struct sockaddr_in* addr) {
    char s[INET_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET, &addr->sin_addr, s, sizeof(s));
    return fastring(s);
}

// get ip string from ipv6 addr
inline fastring ip_str(struct sockaddr_in6* addr) {
    char s[INET6_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET6, &addr->sin6_addr, s, sizeof(s));
    return fastring(s);
}

#ifdef _WIN32
inline int error() { return WSAGetLastError(); }
#else
inline int error() { return errno; }
#endif

// thread-safe strerror
const char* strerror(int err);

inline const char* strerror() {
    return co::strerror(co::error());
}

} // co
