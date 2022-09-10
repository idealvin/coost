#pragma once

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

namespace co {
void disable_hook_sleep();
void enable_hook_sleep();
} // co

// disable hook for android
#ifdef __ANDROID__
#define _CO_DISABLE_HOOK
#endif

#ifdef _CO_DISABLE_HOOK
#define __sys_api(x) ::x

#else
// We have to hook some native APIs, as third-party network libraries may block the 
// coroutine schedulers.
#define __sys_api(x)        _sys_##x
#define _CO_DEC_SYS_API(x)  extern x##_fp_t __sys_api(x)

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h> // for inet_ntop...
#include <MSWSock.h>

#define _CO_DEF_SYS_API(x)  x##_fp_t __sys_api(x) = (x##_fp_t)x

extern "C" {

typedef SOCKET (WINAPI* socket_fp_t)(int, int, int);
typedef int (WINAPI* closesocket_fp_t)(SOCKET);
typedef int (WINAPI* shutdown_fp_t)(SOCKET, int);
typedef int (WINAPI* setsockopt_fp_t)(SOCKET, int, int, const char*, int);
typedef int (WINAPI* ioctlsocket_fp_t)(SOCKET, long, u_long*);
typedef SOCKET(WINAPI* accept_fp_t)(SOCKET, sockaddr*, int*);
typedef int (WINAPI* connect_fp_t)(SOCKET, CONST sockaddr*, int);
typedef int (WINAPI* recv_fp_t)(SOCKET, char*, int, int);
typedef int (WINAPI* recvfrom_fp_t)(SOCKET, char*, int, int, sockaddr*, int*);
typedef int (WINAPI* send_fp_t)(SOCKET, CONST char*, int, int);
typedef int (WINAPI* sendto_fp_t)(SOCKET, CONST char*, int, int, CONST sockaddr*, int);
typedef int (WINAPI* select_fp_t)(int, fd_set*, fd_set*, fd_set*, const timeval*);

typedef void (WINAPI* Sleep_fp_t)(DWORD);
typedef SOCKET(WINAPI* WSASocketA_fp_t)(int, int, int, LPWSAPROTOCOL_INFOA, GROUP, DWORD);
typedef SOCKET(WINAPI* WSASocketW_fp_t)( int, int, int, LPWSAPROTOCOL_INFOW, GROUP, DWORD);
typedef int (WINAPI* WSAIoctl_fp_t)(SOCKET, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int (WINAPI* WSAAsyncSelect_fp_t)(SOCKET, HWND, u_int, long);
typedef int (WINAPI* WSAEventSelect_fp_t)( SOCKET, WSAEVENT, long);
typedef SOCKET(WINAPI* WSAAccept_fp_t)(SOCKET, sockaddr*, LPINT, LPCONDITIONPROC, DWORD_PTR);
typedef int (WINAPI* WSAConnect_fp_t)(SOCKET, CONST sockaddr*, int, LPWSABUF, LPWSABUF, LPQOS, LPQOS);
typedef int (WINAPI* WSARecv_fp_t)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int (WINAPI* WSARecvFrom_fp_t)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, sockaddr*, LPINT, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int (WINAPI* WSASend_fp_t)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int (WINAPI* WSASendTo_fp_t)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, CONST sockaddr*, int, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int (WINAPI* WSARecvMsg_fp_t)(SOCKET, LPWSAMSG, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int (WINAPI* WSASendMsg_fp_t)(SOCKET, LPWSAMSG, DWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int (WINAPI* WSAPoll_fp_t)(LPWSAPOLLFD, ULONG, INT);
typedef DWORD(WINAPI* WSAWaitForMultipleEvents_fp_t)(DWORD, CONST HANDLE*, BOOL, DWORD, BOOL);
typedef BOOL(WINAPI* GetQueuedCompletionStatus_fp_t)(HANDLE, LPDWORD, PULONG_PTR, LPOVERLAPPED*, DWORD);
typedef BOOL(WINAPI* GetQueuedCompletionStatusEx_fp_t)(HANDLE, LPOVERLAPPED_ENTRY, ULONG, PULONG, DWORD, BOOL);

_CO_DEC_SYS_API(socket);
_CO_DEC_SYS_API(closesocket);
_CO_DEC_SYS_API(shutdown);
_CO_DEC_SYS_API(setsockopt);
_CO_DEC_SYS_API(ioctlsocket);
_CO_DEC_SYS_API(accept);
_CO_DEC_SYS_API(connect);
_CO_DEC_SYS_API(recv);
_CO_DEC_SYS_API(recvfrom);
_CO_DEC_SYS_API(send);
_CO_DEC_SYS_API(sendto);
_CO_DEC_SYS_API(select);

_CO_DEC_SYS_API(Sleep);
_CO_DEC_SYS_API(WSASocketA);
_CO_DEC_SYS_API(WSASocketW);
_CO_DEC_SYS_API(WSAIoctl);
_CO_DEC_SYS_API(WSAAsyncSelect);
_CO_DEC_SYS_API(WSAEventSelect);
_CO_DEC_SYS_API(WSAAccept);
_CO_DEC_SYS_API(WSAConnect);
_CO_DEC_SYS_API(WSARecv);
_CO_DEC_SYS_API(WSARecvFrom);
_CO_DEC_SYS_API(WSASend);
_CO_DEC_SYS_API(WSASendTo);
_CO_DEC_SYS_API(WSARecvMsg);
_CO_DEC_SYS_API(WSASendMsg);
_CO_DEC_SYS_API(WSAPoll);
_CO_DEC_SYS_API(WSAWaitForMultipleEvents);
_CO_DEC_SYS_API(GetQueuedCompletionStatus);
_CO_DEC_SYS_API(GetQueuedCompletionStatusEx);

} // "C"

#else

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>  // struct sockaddr_in...
#include <netdb.h>       // getaddrinfo, gethostby...
#include <poll.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <sys/epoll.h>   // epoll
#else
#include <time.h>
#include <sys/event.h>   // kevent
#endif

#define _CO_DEF_SYS_API(x)  x##_fp_t __sys_api(x) = 0

namespace co {

// deduce parameter type of ioctl
template<typename T>
struct ioctl_param;

template<typename X, typename Y>
struct ioctl_param<int(*)(X, Y, ...)> {
    typedef Y type;
};

#if __cplusplus >= 201703L
template<typename X, typename Y>
struct ioctl_param<int(*)(X, Y, ...) noexcept> {
    typedef Y type;
};
#endif

} // co

extern "C" {

typedef int (*socket_fp_t)(int, int, int);
typedef int (*socketpair_fp_t)(int, int, int, int[2]);
typedef int (*pipe_fp_t)(int[2]);
typedef int (*pipe2_fp_t)(int[2], int);
typedef int (*fcntl_fp_t)(int, int, ...);
typedef decltype(ioctl)* ioctl_fp_t;
typedef int (*dup_fp_t)(int);
typedef int (*dup2_fp_t)(int, int);
typedef int (*dup3_fp_t)(int, int, int);
typedef int (*setsockopt_fp_t)(int, int, int, const void*, socklen_t);

typedef int (*close_fp_t)(int);
typedef int (*shutdown_fp_t)(int, int);
typedef int (*connect_fp_t)(int, const struct sockaddr*, socklen_t);
typedef int (*accept_fp_t)(int, struct sockaddr*, socklen_t*);
typedef ssize_t (*read_fp_t)(int, void*, size_t);
typedef ssize_t (*readv_fp_t)(int, const struct iovec*, int);
typedef ssize_t (*recv_fp_t)(int, void*, size_t, int);
typedef ssize_t (*recvfrom_fp_t)(int, void*, size_t, int, struct sockaddr*, socklen_t*);
typedef ssize_t (*recvmsg_fp_t)(int, struct msghdr*, int);
typedef ssize_t (*write_fp_t)(int, const void*, size_t);
typedef ssize_t (*writev_fp_t)(int, const struct iovec*, int);
typedef ssize_t (*send_fp_t)(int, const void*, size_t, int);
typedef ssize_t (*sendto_fp_t)(int, const void*, size_t, int, const struct sockaddr*, socklen_t);
typedef ssize_t (*sendmsg_fp_t)(int, const struct msghdr*, int);
typedef int (*poll_fp_t)(struct pollfd*, nfds_t, int);
typedef int (*select_fp_t)(int, fd_set*, fd_set*, fd_set*, struct timeval*);
typedef unsigned int (*sleep_fp_t)(unsigned int);
typedef int (*usleep_fp_t)(useconds_t);
typedef int (*nanosleep_fp_t)(const struct timespec*, struct timespec*);
typedef struct hostent* (*gethostbyname_fp_t)(const char*);
typedef struct hostent* (*gethostbyaddr_fp_t)(const void*, socklen_t, int);

#ifdef __linux__
typedef int (*epoll_wait_fp_t)(int, struct epoll_event*, int, int);
typedef int (*accept4_fp_t)(int, struct sockaddr*, socklen_t*, int);
typedef struct hostent* (*gethostbyname2_fp_t)(const char*, int);
typedef int (*gethostbyname_r_fp_t)(const char*, struct hostent*, char*, size_t, struct hostent**, int*);
typedef int (*gethostbyname2_r_fp_t)(const char*, int, struct hostent*, char*, size_t, struct hostent**, int*);
typedef int (*gethostbyaddr_r_fp_t)(const void*, socklen_t, int, struct hostent*, char*, size_t, struct hostent**, int*);
#else
typedef int (*kevent_fp_t)(int, const struct kevent*, int, struct kevent*, int, const struct timespec*);
#endif

_CO_DEC_SYS_API(socket);
_CO_DEC_SYS_API(socketpair);
_CO_DEC_SYS_API(pipe);
_CO_DEC_SYS_API(pipe2);
_CO_DEC_SYS_API(fcntl);
_CO_DEC_SYS_API(ioctl);
_CO_DEC_SYS_API(dup);
_CO_DEC_SYS_API(dup2);
_CO_DEC_SYS_API(dup3);
_CO_DEC_SYS_API(setsockopt);

_CO_DEC_SYS_API(close);
_CO_DEC_SYS_API(shutdown);
_CO_DEC_SYS_API(connect);
_CO_DEC_SYS_API(accept);
_CO_DEC_SYS_API(read);
_CO_DEC_SYS_API(readv);
_CO_DEC_SYS_API(recv);
_CO_DEC_SYS_API(recvfrom);
_CO_DEC_SYS_API(recvmsg);
_CO_DEC_SYS_API(write);
_CO_DEC_SYS_API(writev);
_CO_DEC_SYS_API(send);
_CO_DEC_SYS_API(sendto);
_CO_DEC_SYS_API(sendmsg);
_CO_DEC_SYS_API(poll);
_CO_DEC_SYS_API(select);
_CO_DEC_SYS_API(sleep);
_CO_DEC_SYS_API(usleep);
_CO_DEC_SYS_API(nanosleep);
_CO_DEC_SYS_API(gethostbyname);
_CO_DEC_SYS_API(gethostbyaddr);

#ifdef __linux__
_CO_DEC_SYS_API(epoll_wait);
_CO_DEC_SYS_API(accept4);
_CO_DEC_SYS_API(gethostbyname2);
_CO_DEC_SYS_API(gethostbyname_r);
_CO_DEC_SYS_API(gethostbyname2_r);
_CO_DEC_SYS_API(gethostbyaddr_r);
#else
_CO_DEC_SYS_API(kevent);
#endif

} // "C"

#endif // #ifdef _WIN32
#endif // #ifdef _CO_DISABLE_HOOK
