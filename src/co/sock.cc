#ifndef _WIN32

#include "scheduler.h"
#include <unordered_map>

namespace co {

void set_nonblock(sock_t fd) {
    CO_RAW_API(fcntl)(fd, F_SETFL, CO_RAW_API(fcntl)(fd, F_GETFL) | O_NONBLOCK);
}

#ifdef SOCK_NONBLOCK
sock_t socket(int domain, int type, int protocol) {
    return CO_RAW_API(socket)(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
}

#else
sock_t socket(int domain, int type, int protocol) {
    sock_t fd = CO_RAW_API(socket)(domain, type, protocol);
    if (fd != -1) {
        co::set_nonblock(fd);
        co::set_cloexec(fd);
    }
    return fd;
}
#endif

int close(sock_t fd, int ms) {
    if (fd < 0) return CO_RAW_API(close)(fd);
    if (gSched) {
        gSched->del_io_event(fd);
        if (ms > 0) gSched->sleep(ms);
    } else {
        co::get_sock_ctx(fd).del_event();
    }

    int r;
    while ((r = CO_RAW_API(close)(fd)) != 0 && errno == EINTR);
    return r;
}

int shutdown(sock_t fd, char c) {
    if (fd < 0) return CO_RAW_API(shutdown)(fd, SHUT_RDWR);

    if (gSched) {
        if (c == 'r') {
            gSched->del_io_event(fd, ev_read);
            return CO_RAW_API(shutdown)(fd, SHUT_RD);
        } else if (c == 'w') {
            gSched->del_io_event(fd, ev_write);
            return CO_RAW_API(shutdown)(fd, SHUT_WR);
        } else {
            gSched->del_io_event(fd);
            return CO_RAW_API(shutdown)(fd, SHUT_RDWR);
        }

    } else {
        if (c == 'r') {
            co::get_sock_ctx(fd).del_ev_read();
            return CO_RAW_API(shutdown)(fd, SHUT_RD);
        } else if (c == 'w') {
            co::get_sock_ctx(fd).del_ev_write();
            return CO_RAW_API(shutdown)(fd, SHUT_WR);
        } else {
            co::get_sock_ctx(fd).del_event();
            return CO_RAW_API(shutdown)(fd, SHUT_RDWR);
        }
    }
}

int bind(sock_t fd, const void* addr, int addrlen) {
    return ::bind(fd, (const struct sockaddr*)addr, (socklen_t)addrlen);
}

int listen(sock_t fd, int backlog) {
    return ::listen(fd, backlog);
}

sock_t accept(sock_t fd, void* addr, int* addrlen) {
    CHECK(gSched) << "must be called in coroutine..";
    IoEvent ev(fd, ev_read);

    do {
      #ifdef SOCK_NONBLOCK
        sock_t connfd = CO_RAW_API(accept4)(fd, (sockaddr*)addr, (socklen_t*)addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (connfd != -1) return connfd;
      #else
        sock_t connfd = CO_RAW_API(accept)(fd, (sockaddr*)addr, (socklen_t*)addrlen);
        if (connfd != -1) {
            co::set_nonblock(connfd);
            co::set_cloexec(connfd);
            return connfd;
        }
      #endif

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            ev.wait();
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int connect(sock_t fd, const void* addr, int addrlen, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    do {
        int r = CO_RAW_API(connect)(fd, (const sockaddr*)addr, (socklen_t)addrlen);
        if (r == 0) return 0;

        if (errno == EINPROGRESS) {
            IoEvent ev(fd, ev_write);
            if (!ev.wait(ms)) return -1;

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

int recv(sock_t fd, void* buf, int n, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    IoEvent ev(fd, ev_read);

    do {
        int r = (int) CO_RAW_API(recv)(fd, buf, n, 0);
        if (r != -1) return r;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            if (!ev.wait(ms)) return -1;
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int recvn(sock_t fd, void* buf, int n, int ms) {
    char* s = (char*) buf;
    int remain = n;
    IoEvent ev(fd, ev_read);

    do {
        int r = (int) CO_RAW_API(recv)(fd, s, remain, 0);
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

int recvfrom(sock_t fd, void* buf, int n, void* addr, int* addrlen, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    IoEvent ev(fd, ev_read);
    do {
        int r = (int) CO_RAW_API(recvfrom)(fd, buf, n, 0, (sockaddr*)addr, (socklen_t*)addrlen);
        if (r != -1) return r;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            if (!ev.wait(ms)) return -1;
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int send(sock_t fd, const void* buf, int n, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    const char* s = (const char*) buf;
    int remain = n;
    IoEvent ev(fd, ev_write);

    do {
        int r = (int) CO_RAW_API(send)(fd, s, remain, 0);
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

int sendto(sock_t fd, const void* buf, int n, const void* addr, int addrlen, int ms) {
    CHECK(gSched) << "must be called in coroutine..";
    const char* s = (const char*) buf;
    int remain = n;
    IoEvent ev(fd, ev_write);

    do {
        int r = (int) CO_RAW_API(sendto)(fd, s, remain, 0, (const sockaddr*)addr, (socklen_t)addrlen);
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

namespace xx {

class Error {
  public:
    Error() = default;
    ~Error() = default;

    struct T {
        T() : err(4096) {}
        fastream err;
        std::unordered_map<int, uint32> pos;
    };

    const char* strerror(int e) {
        if (_p == NULL) _p.reset(new T);
        auto it = _p->pos.find(e);
        if (it != _p->pos.end()) {
            return _p->err.data() + it->second;
        } else {
            uint32 pos = (uint32) _p->err.size();
            static ::Mutex mtx;
            {
                ::MutexGuard g(mtx);
                _p->err.append(::strerror(e)).append('\0');
            }
            _p->pos[e] = pos;
            return _p->err.data() + pos;
        }
    }

  private:
    thread_ptr<T> _p;
};

} // xx

const char* strerror(int err) {
    if (err == ETIMEDOUT) return "Timed out";
    static xx::Error e;
    return e.strerror(err);
}

} // co

#endif
