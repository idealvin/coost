#pragma once

// disable hook for ios and android
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR || defined(__ANDROID__)
#define CO_DISABLE_HOOK
#endif

#ifdef CO_DISABLE_HOOK
#define CO_RAW_API(x) ::x
#else

// We have to hook some native APIs, as third-party network libraries may block the 
// coroutine schedulers.
#define CO_RAW_API(x)         co_raw_##x
#define CO_DEC_RAW_API(x)     extern x##_fp_t CO_RAW_API(x)

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h> // for inet_ntop...
#include <MSWSock.h>

#define CO_DEF_RAW_API(x)  x##_fp_t CO_RAW_API(x) = x

extern "C" {

typedef SOCKET (WINAPI* socket_fp_t)(
    int a0,
    int a1,
    int a2
);

typedef SOCKET(WINAPI* WSASocketA_fp_t)(
    int                 a0,
    int                 a1,
    int                 a2,
    LPWSAPROTOCOL_INFOA a3,
    GROUP               a4,
    DWORD               a5
);

typedef SOCKET(WINAPI* WSASocketW_fp_t)(
    int                 a0,
    int                 a1,
    int                 a2,
    LPWSAPROTOCOL_INFOW a3,
    GROUP               a4,
    DWORD               a5
);

typedef int (WINAPI* closesocket_fp_t)(
    SOCKET a0
);

typedef int (WINAPI* shutdown_fp_t)(
    SOCKET a0,
    int    a1
);

typedef int (WINAPI* setsockopt_fp_t)(
    SOCKET      a0,
    int         a1,
    int         a2,
    const char* a3,
    int         a4
);

typedef int (WINAPI* ioctlsocket_fp_t)(
    SOCKET  a0,
    long    a1,
    u_long* a2
);

typedef int (WINAPI* WSAIoctl_fp_t)(
    SOCKET                             a0,
    DWORD                              a1,
    LPVOID                             a2,
    DWORD                              a3,
    LPVOID                             a4,
    DWORD                              a5,
    LPDWORD                            a6,
    LPWSAOVERLAPPED                    a7,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a8
);

typedef int (WINAPI* WSAAsyncSelect_fp_t)(
    SOCKET a0,
    HWND   a1,
    u_int  a2,
    long   a3
);

typedef int (WINAPI* WSAEventSelect_fp_t)(
    SOCKET   a0,
    WSAEVENT a1,
    long     a2
);

typedef SOCKET(WINAPI* accept_fp_t)(
    SOCKET    a0,
    sockaddr* a1,
    int*      a2
);

typedef SOCKET(WINAPI* WSAAccept_fp_t)(
    SOCKET a0,
    sockaddr* a1,
    LPINT a2,
    LPCONDITIONPROC a3,
    DWORD_PTR a4
);

typedef int (WINAPI* connect_fp_t)(
    SOCKET a0,
    CONST sockaddr* a1,
    int a2
);

typedef int (WINAPI* WSAConnect_fp_t)(
    SOCKET a0,
    CONST sockaddr* a1,
    int a2,
    LPWSABUF a3,
    LPWSABUF a4,
    LPQOS a5,
    LPQOS a6
);

typedef int (WINAPI* recv_fp_t)(
    SOCKET a0,
    char* a1,
    int a2,
    int a3
);

typedef int (WINAPI* WSARecv_fp_t)(
    SOCKET a0,
    LPWSABUF a1,
    DWORD a2,
    LPDWORD a3,
    LPDWORD a4,
    LPWSAOVERLAPPED a5,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a6
);

typedef int (WINAPI* recvfrom_fp_t)(
    SOCKET a0,
    char* a1,
    int a2,
    int a3,
    sockaddr* a4,
    int* a5
);

typedef int (WINAPI* WSARecvFrom_fp_t)(
    SOCKET a0,
    LPWSABUF a1,
    DWORD a2,
    LPDWORD a3,
    LPDWORD a4,
    sockaddr* a5,
    LPINT a6,
    LPWSAOVERLAPPED a7,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a8
);

typedef int (WINAPI* send_fp_t)(
    SOCKET a0,
    CONST char* a1,
    int a2,
    int a3
);

typedef int (WINAPI* WSASend_fp_t)(
    SOCKET a0,
    LPWSABUF a1,
    DWORD a2,
    LPDWORD a3,
    DWORD a4,
    LPWSAOVERLAPPED a5,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a6
);

typedef int (WINAPI* sendto_fp_t)(
    SOCKET a0,
    CONST char* a1,
    int a2,
    int a3,
    CONST sockaddr* a4,
    int a5
);

typedef int (WINAPI* WSASendTo_fp_t)(
    SOCKET a0,
    LPWSABUF a1,
    DWORD a2,
    LPDWORD a3,
    DWORD a4,
    CONST sockaddr* a5,
    int a6,
    LPWSAOVERLAPPED a7,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a8
);

typedef int (WINAPI* WSARecvMsg_fp_t)(
    SOCKET                             a0,
    LPWSAMSG                           a1,
    LPDWORD                            a2,
    LPWSAOVERLAPPED                    a3,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a4
);

typedef int (WINAPI* WSASendMsg_fp_t)(
    SOCKET                             a0,
    LPWSAMSG                           a1,
    DWORD                              a2,
    LPDWORD                            a3,
    LPWSAOVERLAPPED                    a4,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE a5
);

typedef int (WINAPI* select_fp_t)(
    int a0,
    fd_set* a1,
    fd_set* a2,
    fd_set* a3,
    const timeval* a4
);

typedef int (WINAPI* WSAPoll_fp_t)(
    LPWSAPOLLFD a0,
    ULONG a1,
    INT a2
);

typedef DWORD(WINAPI* WSAWaitForMultipleEvents_fp_t)(
    DWORD a0,
    CONST HANDLE* a1,
    BOOL a2,
    DWORD a3,
    BOOL a4
);

typedef BOOL(WINAPI* GetQueuedCompletionStatus_fp_t)(
    HANDLE        a0,
    LPDWORD       a1,
    PULONG_PTR    a2,
    LPOVERLAPPED* a3,
    DWORD         a4
);

typedef BOOL(WINAPI* GetQueuedCompletionStatusEx_fp_t)(
    HANDLE             a0,
    LPOVERLAPPED_ENTRY a1,
    ULONG              a2,
    PULONG             a3,
    DWORD              a4,
    BOOL               a5
);

CO_DEC_RAW_API(socket);
CO_DEC_RAW_API(WSASocketA);
CO_DEC_RAW_API(WSASocketW);
CO_DEC_RAW_API(closesocket);
CO_DEC_RAW_API(shutdown);
CO_DEC_RAW_API(setsockopt);
CO_DEC_RAW_API(ioctlsocket);
CO_DEC_RAW_API(WSAIoctl);
CO_DEC_RAW_API(WSAAsyncSelect);
CO_DEC_RAW_API(WSAEventSelect);
CO_DEC_RAW_API(accept);
CO_DEC_RAW_API(WSAAccept);
CO_DEC_RAW_API(connect);
CO_DEC_RAW_API(WSAConnect);
CO_DEC_RAW_API(recv);
CO_DEC_RAW_API(WSARecv);
CO_DEC_RAW_API(recvfrom);
CO_DEC_RAW_API(WSARecvFrom);
CO_DEC_RAW_API(send);
CO_DEC_RAW_API(WSASend);
CO_DEC_RAW_API(sendto);
CO_DEC_RAW_API(WSASendTo);
CO_DEC_RAW_API(WSARecvMsg);
CO_DEC_RAW_API(WSASendMsg);
CO_DEC_RAW_API(select);
CO_DEC_RAW_API(WSAPoll);
CO_DEC_RAW_API(WSAWaitForMultipleEvents);
CO_DEC_RAW_API(GetQueuedCompletionStatus);
CO_DEC_RAW_API(GetQueuedCompletionStatusEx);

void co_attach_hooks();
void co_detach_hooks();

} // "C"

#else

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>  // struct sockaddr_in...
#include <netdb.h>       // getaddrinfo, gethostby...
#include <poll.h>
#include <sys/select.h>
#ifdef __linux__
#include <sys/epoll.h>   // epoll
#else
#include <time.h>
#include <sys/event.h>   // kevent
#endif

#define CO_DEF_RAW_API(x)  x##_fp_t CO_RAW_API(x) = 0

extern "C" {

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

CO_DEC_RAW_API(close);
CO_DEC_RAW_API(shutdown);
CO_DEC_RAW_API(connect);
CO_DEC_RAW_API(accept);
CO_DEC_RAW_API(read);
CO_DEC_RAW_API(readv);
CO_DEC_RAW_API(recv);
CO_DEC_RAW_API(recvfrom);
CO_DEC_RAW_API(recvmsg);
CO_DEC_RAW_API(write);
CO_DEC_RAW_API(writev);
CO_DEC_RAW_API(send);
CO_DEC_RAW_API(sendto);
CO_DEC_RAW_API(sendmsg);
CO_DEC_RAW_API(poll);
CO_DEC_RAW_API(select);
CO_DEC_RAW_API(sleep);
CO_DEC_RAW_API(usleep);
CO_DEC_RAW_API(nanosleep);
CO_DEC_RAW_API(gethostbyname);
CO_DEC_RAW_API(gethostbyaddr);

#ifdef __linux__
CO_DEC_RAW_API(epoll_wait);
CO_DEC_RAW_API(accept4);
CO_DEC_RAW_API(gethostbyname2);
CO_DEC_RAW_API(gethostbyname_r);
CO_DEC_RAW_API(gethostbyname2_r);
CO_DEC_RAW_API(gethostbyaddr_r);
#else
CO_DEC_RAW_API(kevent);
#endif

} // "C"

#endif // #ifdef _WIN32
#endif // #ifdef CO_DISABLE_HOOK
