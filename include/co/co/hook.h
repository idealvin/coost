#pragma once

#ifndef _WIN32

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

extern "C" {

/**
 * We have to hook some native APIs, as third-party network libraries may block our 
 * coroutine schedulers.
 */

// strerror is hooked as it is not thread-safe. 
typedef char* (*strerror_fp_t)(int);

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

#define raw_api(x) raw_##x
#define dec_raw_api(x) extern x##_fp_t raw_api(x)
#define def_raw_api(x) x##_fp_t raw_api(x) = 0

/**
 * Declare raw API function pointers. We can use these pointers to call 
 * the native API directly. The new name is raw_ + original name.
 */
dec_raw_api(strerror);
dec_raw_api(close);
dec_raw_api(shutdown);
dec_raw_api(connect);
dec_raw_api(accept);
dec_raw_api(read);
dec_raw_api(readv);
dec_raw_api(recv);
dec_raw_api(recvfrom);
dec_raw_api(recvmsg);
dec_raw_api(write);
dec_raw_api(writev);
dec_raw_api(send);
dec_raw_api(sendto);
dec_raw_api(sendmsg);
dec_raw_api(poll);
dec_raw_api(select);
dec_raw_api(sleep);
dec_raw_api(usleep);
dec_raw_api(nanosleep);
dec_raw_api(gethostbyname);
dec_raw_api(gethostbyaddr);

#ifdef __linux__
dec_raw_api(epoll_wait);
dec_raw_api(accept4);
dec_raw_api(gethostbyname2);
dec_raw_api(gethostbyname_r);
dec_raw_api(gethostbyname2_r);
dec_raw_api(gethostbyaddr_r);
#else
dec_raw_api(kevent);
#endif

} // "C"

#endif
