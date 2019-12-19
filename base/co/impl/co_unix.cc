#ifndef _WIN32

#include "scheduler.h"
#include "hook.h"

DEF_int32(tcp_max_recv_size, 1024 * 1024, "max size for a single recv");
DEF_int32(tcp_max_send_size, 1024 * 1024, "max size for a single send");

namespace co {

#ifdef SOCK_NONBLOCK
sock_t tcp_socket(int v) {
    return ::socket((v == 4 ? AF_INET : AF_INET6), SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
}

sock_t udp_socket(int v) {
    return ::socket((v == 4 ? AF_INET : AF_INET6), SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
}

#else
sock_t tcp_socket(int v) {
    sock_t fd = ::socket((v == 4 ? AF_INET : AF_INET6), SOCK_STREAM, IPPROTO_TCP);
    if (fd != -1) {
        co::set_nonblock(fd);
        co::set_cloexec(fd);
    }
    return fd;
}

sock_t udp_socket(int v) {
    sock_t fd = ::socket((v == 4 ? AF_INET : AF_INET6), SOCK_DGRAM, IPPROTO_UDP);
    if (fd != -1) {
        co::set_nonblock(fd);
        co::set_cloexec(fd);
    }
    return fd;
}
#endif

int close(sock_t fd) {
    gSched->del_event(fd);
    int r;
    while ((r = fp_close(fd)) != 0 && errno == EINTR);
    return r;
}

int close(sock_t fd, int ms) {
    gSched->del_event(fd);
    gSched->sleep(ms);
    int r;
    while ((r = fp_close(fd)) != 0 && errno == EINTR);
    return r;
}

int shutdown(sock_t fd, char c) {
    if (c == 'r') {
        gSched->del_ev_read(fd);
        return fp_shutdown(fd, SHUT_RD);
    } else if (c == 'w') {
        gSched->del_ev_write(fd);
        return fp_shutdown(fd, SHUT_WR);
    } else {
        gSched->del_event(fd);
        return fp_shutdown(fd, SHUT_RDWR);
    }
}

int bind(sock_t fd, const void* addr, int addrlen) {
    return ::bind(fd, (const struct sockaddr*) addr, (socklen_t) addrlen);
}

int listen(sock_t fd, int backlog) {
    return ::listen(fd, backlog);
}

sock_t accept(sock_t fd, void* addr, int* addrlen) {
    do {
      #ifdef SOCK_NONBLOCK
        sock_t conn_fd = fp_accept4(fd, (sockaddr*)addr, (socklen_t*)addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (conn_fd != -1) return conn_fd;
      #else
        sock_t conn_fd = fp_accept(fd, (sockaddr*)addr, (socklen_t*)addrlen);
        if (conn_fd != -1) {
            co::set_nonblock(conn_fd);
            co::set_cloexec(conn_fd);
            return conn_fd;
        }
      #endif

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            gSched->add_ev_read(fd);
            gSched->yield();
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int connect(sock_t fd, const void* addr, int addrlen) {
    do {
        int r = fp_connect(fd, (const sockaddr*)addr, (socklen_t)addrlen);
        if (r == 0) return 0;

        if (errno == EINPROGRESS) {
            gSched->add_ev_write(fd);
            gSched->yield();
            gSched->del_event(fd);

            int err, len = sizeof(err);
            r = co::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
            if (r != 0) return -1;
            if (err == 0) return 0;
            errno = err;
            return -1;
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int connect(sock_t fd, const void* addr, int addrlen, int ms) {
    do {
        int r = fp_connect(fd, (const sockaddr*)addr, (socklen_t)addrlen);
        if (r == 0) return 0;

        if (errno == EINPROGRESS) {
            EvWrite ev(fd);
            if (ev.wait(ms, ETIMEDOUT)) {
                int err, len = sizeof(err);
                r = co::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
                if (r != 0) return -1;
                if (err == 0) return 0;
                errno = err;
                return -1;
            } else {
                return -1;
            }

        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int recv(sock_t fd, void* buf, int n) {
    do {
        int r = (int) fp_recv(fd, buf, n, 0);
        if (r != -1) return r;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            gSched->add_ev_read(fd);
            gSched->yield();
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int recv(sock_t fd, void* buf, int n, int ms) {
    EvRead ev(fd);

    do {
        int r = (int) fp_recv(fd, buf, n, 0);
        if (r != -1) return r;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            if (!ev.wait(ms)) return -1;
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int recvn(sock_t fd, void* buf, int n) {
    char* s = (char*) buf;
    int remain = n;
    do {
        int r = (int) fp_recv(fd, s, remain, 0);
        if (r == remain) return n;
        if (r == 0) return 0;

        if (r == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                gSched->add_ev_read(fd);
                gSched->yield();
            } else if (errno != EINTR) {
                return -1;
            }
        } else {
            remain -= r;
            s += r;
        }
    } while (true);
}

int _Recvn(sock_t fd, void* buf, int n, int ms) {
    char* s = (char*) buf;
    int remain = n;
    EvRead ev(fd);

    do {
        int r = (int) fp_recv(fd, s, remain, 0);
        if (r == remain) return n;
        if (r == 0) return 0;

        if (r == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                if (!ev.wait(ms)) return -1;
            } else if (errno != EINTR) {
                return -1;
            }
        } else {
            remain -= r;
            s += r;
        }
    } while (true);
}

int recvn(sock_t fd, void* buf, int n, int ms) {
    char* s = (char*) buf;
    int remain = n;

    while (remain > FLG_tcp_max_recv_size) {
        int r = _Recvn(fd, s, FLG_tcp_max_recv_size, ms);
        if (r != FLG_tcp_max_recv_size) return r;
        remain -= FLG_tcp_max_recv_size;
        s += FLG_tcp_max_recv_size;
    }

    int r = _Recvn(fd, s, remain, ms);
    return r != remain ? r : n;
}

int recvfrom(sock_t fd, void* buf, int n, void* addr, int* addrlen) {
    do {
        int r = (int) fp_recvfrom(fd, buf, n, 0, (sockaddr*)addr, (socklen_t*)addrlen);
        if (r != -1) return r;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            gSched->add_ev_read(fd);
            gSched->yield();
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int recvfrom(sock_t fd, void* buf, int n, void* addr, int* addrlen, int ms) {
    EvRead ev(fd);
    do {
        int r = (int) fp_recvfrom(fd, buf, n, 0, (sockaddr*)addr, (socklen_t*)addrlen);
        if (r != -1) return r;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            if (!ev.wait(ms)) return -1;
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int send(sock_t fd, const void* buf, int n) {
    const char* s = (const char*) buf;
    int remain = n;
    EvWrite ev(fd);

    do {
        int r = (int) fp_send(fd, s, remain, 0);
        if (r == remain) return n;

        if (r == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                ev.wait();
            } else if (errno != EINTR) {
                return -1;
            }
        } else {
            remain -= r;
            s += r;
        }
    } while (true);
}

int _Send(sock_t fd, const void* buf, int n, int ms) {
    const char* s = (const char*) buf;
    int remain = n;
    EvWrite ev(fd);

    do {
        int r = (int) fp_send(fd, s, remain, 0);
        if (r == remain) return n;

        if (r == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                if (!ev.wait(ms)) return -1;
            } else if (errno != EINTR) {
                return -1;
            }
        } else {
            remain -= r;
            s += r;
        }
    } while (true);
}

int send(sock_t fd, const void* buf, int n, int ms) {
    const char* s = (const char*) buf;
    int remain = n;

    while (remain > FLG_tcp_max_send_size) {
        int r = _Send(fd, s, FLG_tcp_max_send_size, ms);
        if (r != FLG_tcp_max_send_size) return r;
        remain -= FLG_tcp_max_send_size;
        s += FLG_tcp_max_send_size;
    }

    int r = _Send(fd, s, remain, ms);
    return r != remain ? r : n;
}

int sendto(sock_t fd, const void* buf, int n, const void* addr, int addrlen) {
    do {
        int r = (int) fp_sendto(fd, buf, n, 0, (const sockaddr*)addr, (socklen_t)addrlen);
        if (r != -1) return r;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            gSched->add_ev_write(fd);
            gSched->yield();
            gSched->del_ev_write(fd);
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int sendto(sock_t fd, const void* buf, int n, const void* addr, int addrlen, int ms) {
    EvWrite ev(fd);

    do {
        int r = (int) fp_sendto(fd, buf, n, 0, (const sockaddr*)addr, (socklen_t)addrlen);
        if (r != -1) return r;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            if (!ev.wait(ms)) return -1;
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

const char* strerror(int err) {
    static __thread std::unordered_map<int, const char*>* kErrStr = 0;
    if (!kErrStr) kErrStr = new std::unordered_map<int, const char*>();

    if (err == ETIMEDOUT) return "timedout";
    auto& e = (*kErrStr)[err];
    if (e) return e;

    static ::Mutex mtx;
    ::MutexGuard g(mtx);
    return e = strdup(::strerror(err));
}

} // co

#endif
