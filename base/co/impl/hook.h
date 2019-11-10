#pragma once

#ifndef _WIN32

#include "../co.h"
#include <poll.h>
#include <sys/select.h>
#ifdef __linux__
#include <sys/epoll.h>
#else
#include <time.h>
#include <sys/event.h>
#endif

extern "C" {

typedef int (*connect_fp_t)(int, const struct sockaddr*, socklen_t);
typedef int (*accept_fp_t)(int, struct sockaddr*, socklen_t*);
typedef int (*close_fp_t)(int);
typedef int (*shutdown_fp_t)(int, int);

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

extern connect_fp_t fp_connect;
extern accept_fp_t fp_accept;
extern close_fp_t fp_close;
extern shutdown_fp_t fp_shutdown;

extern read_fp_t fp_read;
extern readv_fp_t fp_readv;
extern recv_fp_t fp_recv;
extern recvfrom_fp_t fp_recvfrom;
extern recvmsg_fp_t fp_recvmsg;

extern write_fp_t fp_write;
extern writev_fp_t fp_writev;
extern send_fp_t fp_send;
extern sendto_fp_t fp_sendto;
extern sendmsg_fp_t fp_sendmsg;

extern poll_fp_t fp_poll;
extern select_fp_t fp_select;

extern sleep_fp_t fp_sleep;
extern usleep_fp_t fp_usleep;
extern nanosleep_fp_t fp_nanosleep;

extern gethostbyname_fp_t fp_gethostbyname;
extern gethostbyaddr_fp_t fp_gethostbyaddr;

#ifdef __linux__
extern epoll_wait_fp_t fp_epoll_wait;
extern accept4_fp_t fp_accept4;
extern gethostbyname2_fp_t fp_gethostbyname2;
extern gethostbyname_r_fp_t fp_gethostbyname_r;
extern gethostbyname2_r_fp_t fp_gethostbyname2_r;
extern gethostbyaddr_r_fp_t fp_gethostbyaddr_r;
#else
extern kevent_fp_t fp_kevent;
#endif

} // "C"

#endif
