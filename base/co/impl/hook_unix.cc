#include "hook.h"
#include "scheduler.h"
#include <dlfcn.h>

namespace co {

class HookInfo {
  public:
    explicit HookInfo(int64 v = 0) : _v(v) {}
    ~HookInfo() = default;

    HookInfo(const HookInfo& h) {
        _v = h._v;
    }

    bool hookable() const {
        return _v != 0;
    }

    int send_timeout() const {
        return _p.send_timeout;
    }

    int recv_timeout() const {
        return _p.recv_timeout;
    }

    void set_send_timeout(int ms) {
        _p.send_timeout = ms;
    }

    void set_recv_timeout(int ms) {
        _p.recv_timeout = ms;
    }

    int timeout(char c) {
        return c == 'r' ? recv_timeout() : send_timeout();
    }

    void set_timeout(int ms, char c) {
        c == 'r' ? set_recv_timeout(ms) : set_send_timeout(ms);
    }

  private:
    union {
        struct {
            int32 send_timeout;
            int32 recv_timeout;
        } _p;

        int64 _v;
    };
};

class Hook {
  public:
    Hook() : _hk(FLG_co_sched_num) {}
    ~Hook() = default;

    void erase(int fd) {
        _hk[gSched->id()].erase(fd);
    }

    HookInfo find_and_erase(int fd) {
        auto& hk = _hk[gSched->id()];
        auto it = hk.find(fd);
        if (it == hk.end()) return HookInfo(0);

        HookInfo hi = it->second;
        hk.erase(it);
        return hi;
    }

    HookInfo get_hook_info(int fd, char c=0) {
        auto& hk = _hk[gSched->id()];
        auto it = hk.find(fd);
        if (it != hk.end()) {
            HookInfo& hi = it->second;
            if (hi.hookable() && c && hi.timeout(c) == 0) {
                hi.set_timeout(this->_Get_timeout(fd, c), c);
            }
            return hi;
        }

        int flag = fcntl(fd, F_GETFL);
        if (flag & O_NONBLOCK) return hk[fd] = HookInfo(0);

        int ms = this->_Get_timeout(fd, c);
        if (ms == 0) return hk[fd] = HookInfo(0); // not socket

        HookInfo hi;
        hi.set_timeout(ms, c);
        fcntl(fd, F_SETFL, flag | O_NONBLOCK);
        return hk[fd] = hi;
    }

  private:
    std::vector<std::unordered_map<int, HookInfo>> _hk;

    int _Get_timeout(int fd, char c) {
        struct timeval tv;
        int len = sizeof(tv);
        int opt = (c == 'r' ? SO_RCVTIMEO : SO_SNDTIMEO);
        int r = co::getsockopt(fd, SOL_SOCKET, opt, &tv, &len);
        if (r == 0) {
            int ms = (int) (tv.tv_sec * 1000 + tv.tv_usec / 1000);
            return ms > 0 ? ms : -1;
        }
        return 0; // fd is not valid, or fd does not refer to a socket
    }
};

} // co

inline co::Hook& gHook() {
    static co::Hook hook;
    return hook;
}

using co::gSched;
using co::EvRead;
using co::EvWrite;

#ifndef __linux__
co::Mutex gDnsMtx;
#endif

inline struct hostent* gHostEnt() {
    static std::vector<struct hostent> ents(FLG_co_sched_num);
    return &ents[gSched->id()];
}

inline co::Mutex& gDnsMutex() {
    static std::vector<co::Mutex> mtx(FLG_co_sched_num);
    return mtx[gSched->id()];
}


extern "C" {

connect_fp_t fp_connect = 0;
accept_fp_t fp_accept = 0;
close_fp_t fp_close = 0;
shutdown_fp_t fp_shutdown = 0;

read_fp_t fp_read = 0;
readv_fp_t fp_readv = 0;
recv_fp_t fp_recv = 0;
recvfrom_fp_t fp_recvfrom = 0;
recvmsg_fp_t fp_recvmsg = 0;

write_fp_t fp_write = 0;
writev_fp_t fp_writev = 0;
send_fp_t fp_send = 0;
sendto_fp_t fp_sendto = 0;
sendmsg_fp_t fp_sendmsg = 0;

poll_fp_t fp_poll = 0;
select_fp_t fp_select = 0;

sleep_fp_t fp_sleep = 0;
usleep_fp_t fp_usleep = 0;
nanosleep_fp_t fp_nanosleep = 0;

gethostbyname_fp_t fp_gethostbyname = 0;
gethostbyaddr_fp_t fp_gethostbyaddr = 0;

#ifdef __linux__
epoll_wait_fp_t fp_epoll_wait = 0;
accept4_fp_t fp_accept4 = 0;
gethostbyname2_fp_t fp_gethostbyname2 = 0;
gethostbyname_r_fp_t fp_gethostbyname_r = 0;
gethostbyname2_r_fp_t fp_gethostbyname2_r = 0;
gethostbyaddr_r_fp_t fp_gethostbyaddr_r = 0;
#else
kevent_fp_t fp_kevent = 0;
#endif


int connect(int fd, const struct sockaddr* addr, socklen_t addrlen) {
    if (!gSched) return fp_connect(fd, addr, addrlen);

    auto hi = gHook().get_hook_info(fd, 'w'); // 'w' for send timeout
    if (!hi.hookable()) return fp_connect(fd, addr, addrlen);

    int r;
    if (hi.send_timeout() < 0) {
        r = co::connect(fd, addr, addrlen);
    } else {
        r = co::connect(fd, addr, addrlen, hi.send_timeout());
        // set errno to EINPROGRESS if SO_SNDTIMEO is set and the timeout expires
        if (r == -1 && errno == ETIMEDOUT) errno = EINPROGRESS;
    }

    gHook().erase(fd);
    return r;
}

int accept(int fd, struct sockaddr* addr, socklen_t* addrlen) {
    if (!gSched) return fp_accept(fd, addr, addrlen);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_accept(fd, addr, addrlen);

    do {
        int conn_fd = fp_accept(fd, addr, addrlen);
        if (conn_fd != -1) return conn_fd;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            gSched->add_ev_read(fd);
            gSched->yield();
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int close(int fd) {
    if (!gSched) return fp_close(fd);

    auto hi = gHook().find_and_erase(fd);
    if (!hi.hookable()) return fp_close(fd);
    return co::close(fd);
}

int __close(int fd) {
    return close(fd);
}

#define do_hook(f, ev) \
    do { \
        auto r = f; \
        if (r != -1) return r; \
        if (errno == EWOULDBLOCK || errno == EAGAIN) { \
            ev.wait(); \
        } else if (errno != EINTR) { \
            return -1; \
        } \
    } while (true)

#define do_hook_ms(f, ev, ms) \
    do { \
        auto r = f; \
        if (r != -1) return r; \
        if (errno == EWOULDBLOCK || errno == EAGAIN) { \
            if (!ev.wait(ms, EWOULDBLOCK)) return -1; \
        } else if (errno != EINTR) { \
            return -1; \
        } \
    } while (true)


ssize_t read(int fd, void* buf, size_t count) {
    if (!gSched) return fp_read(fd, buf, count);

    auto hi = gHook().get_hook_info(fd, 'r');
    if (!hi.hookable()) return fp_read(fd, buf, count);

    EvRead ev(fd);
    if (hi.recv_timeout() < 0) {
        do_hook(fp_read(fd, buf, count), ev);
    } else {
        do_hook_ms(fp_read(fd, buf, count), ev, hi.recv_timeout());
    }
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
    if (!gSched) return fp_readv(fd, iov, iovcnt);

    auto hi = gHook().get_hook_info(fd, 'r');
    if (!hi.hookable()) return fp_readv(fd, iov, iovcnt);

    EvRead ev(fd);
    if (hi.recv_timeout() < 0) {
        do_hook(fp_readv(fd, iov, iovcnt), ev);
    } else {
        do_hook_ms(fp_readv(fd, iov, iovcnt), ev, hi.recv_timeout());
    }
}

ssize_t recv(int fd, void* buf, size_t len, int flags) {
    if (!gSched) return fp_recv(fd, buf, len, flags);

    auto hi = gHook().get_hook_info(fd, 'r');
    if (!hi.hookable()) return fp_recv(fd, buf, len, flags);

    EvRead ev(fd);
    if (hi.recv_timeout() < 0) {
        do_hook(fp_recv(fd, buf, len, flags), ev);
    } else {
        do_hook_ms(fp_recv(fd, buf, len, flags), ev, hi.recv_timeout());
    }
}

ssize_t recvfrom(int fd, void* buf, size_t len, int flags, struct sockaddr* addr, socklen_t* addrlen) {
    if (!gSched) return fp_recvfrom(fd, buf, len, flags, addr, addrlen);

    auto hi = gHook().get_hook_info(fd, 'r');
    if (!hi.hookable()) return fp_recvfrom(fd, buf, len, flags, addr, addrlen);

    EvRead ev(fd);
    if (hi.recv_timeout() < 0) {
        do_hook(fp_recvfrom(fd, buf, len, flags, addr, addrlen), ev);
    } else {
        do_hook_ms(fp_recvfrom(fd, buf, len, flags, addr, addrlen), ev, hi.recv_timeout());
    }
}

ssize_t recvmsg(int fd, struct msghdr* msg, int flags) {
    if (!gSched) return fp_recvmsg(fd, msg, flags);

    auto hi = gHook().get_hook_info(fd, 'r');
    if (!hi.hookable()) return fp_recvmsg(fd, msg, flags);

    EvRead ev(fd);
    if (hi.recv_timeout() < 0) {
        do_hook(fp_recvmsg(fd, msg, flags), ev);
    } else {
        do_hook_ms(fp_recvmsg(fd, msg, flags), ev, hi.recv_timeout());
    }
}

ssize_t write(int fd, const void* buf, size_t count) {
    if (!gSched) return fp_write(fd, buf, count);

    auto hi = gHook().get_hook_info(fd, 'w');
    if (!hi.hookable()) return fp_write(fd, buf, count);

    EvWrite ev(fd);
    if (hi.send_timeout() < 0) {
        do_hook(fp_write(fd, buf, count), ev);
    } else {
        do_hook_ms(fp_write(fd, buf, count), ev, hi.send_timeout());
    }
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    if (!gSched) return fp_writev(fd, iov, iovcnt);

    auto hi = gHook().get_hook_info(fd, 'w');
    if (!hi.hookable()) return fp_writev(fd, iov, iovcnt);

    EvWrite ev(fd);
    if (hi.send_timeout() < 0) {
        do_hook(fp_writev(fd, iov, iovcnt), ev);
    } else {
        do_hook_ms(fp_writev(fd, iov, iovcnt), ev, hi.send_timeout());
    }
}

ssize_t send(int fd, const void* buf, size_t len, int flags) {
    if (!gSched) return fp_send(fd, buf, len, flags);

    auto hi = gHook().get_hook_info(fd, 'w');
    if (!hi.hookable()) return fp_send(fd, buf, len, flags);

    EvWrite ev(fd);
    if (hi.send_timeout() < 0) {
        do_hook(fp_send(fd, buf, len, flags), ev);
    } else {
        do_hook_ms(fp_send(fd, buf, len, flags), ev, hi.send_timeout());
    }
}

ssize_t sendto(int fd, const void* buf, size_t len, int flags, const struct sockaddr* addr, socklen_t addrlen) {
    if (!gSched) return fp_sendto(fd, buf, len, flags, addr, addrlen);

    auto hi = gHook().get_hook_info(fd, 'w');
    if (!hi.hookable()) return fp_sendto(fd, buf, len, flags, addr, addrlen);

    EvWrite ev(fd);
    if (hi.send_timeout() < 0) {
        do_hook(fp_sendto(fd, buf, len, flags, addr, addrlen), ev);
    } else {
        do_hook_ms(fp_sendto(fd, buf, len, flags, addr, addrlen), ev, hi.send_timeout());
    }
}

ssize_t sendmsg(int fd, const struct msghdr* msg, int flags) {
    if (!gSched) return fp_sendmsg(fd, msg, flags);

    auto hi = gHook().get_hook_info(fd, 'w');
    if (!hi.hookable()) return fp_sendmsg(fd, msg, flags);

    EvWrite ev(fd);
    if (hi.send_timeout() < 0) {
        do_hook(fp_sendmsg(fd, msg, flags), ev);
    } else {
        do_hook_ms(fp_sendmsg(fd, msg, flags), ev, hi.send_timeout());
    }
}

int poll(struct pollfd* fds, nfds_t nfds, int ms) {
    if (!gSched || ms == 0) return fp_poll(fds, nfds, ms);

    do {
        if (nfds == 1) {
            int fd = fds[0].fd;
            if (fds[0].events == POLLIN) {
                if (!gSched->add_ev_read(fd)) break;
            } else if (fds[0].events == POLLOUT) {
                if (!gSched->add_ev_write(fd)) break;
            } else {
                break;
            }

            co::timer_id_t id = co::null_timer_id;
            if (ms > 0) id = gSched->add_ev_timer(ms);

            gSched->yield();
            if (ms > 0 && gSched->timeout()) {
                gSched->del_event(fd);
                return 0;
            }

            if (id != co::null_timer_id) gSched->del_timer(id);
            if (fds[0].events == POLLOUT) gSched->del_ev_write(fd);

            fds[0].revents = fds[0].events;
            return 1;
        }
    } while (0);

    // it's boring to hook poll when nfds > 1, just check poll every 16 ms
    do {
        int r = fp_poll(fds, nfds, 0);
        if (r != 0) return r;
        co::sleep(16);
        if (ms > 0 && (ms -= 16) < 0) return 0;
    } while (true);
}

int __poll(struct pollfd* fds, nfds_t nfds, int ms) {
    return poll(fds, nfds, ms);
}

int select(int nfds, fd_set* r, fd_set* w, fd_set* e, struct timeval* tv) {
    if (!gSched) return fp_select(nfds, r, w, e, tv);

    int ms = -1;
    if (tv) ms = (int) (tv->tv_sec * 1000 + tv->tv_usec / 1000);
    if (ms == 0) return fp_select(nfds, r, w, e, tv);

    if ((nfds == 0 || (!r && !w && !e)) && ms > 0) {
        co::sleep(ms);
        return 0;
    }

    // it's boring to hook select, just check select every 16 ms
    struct timeval o = { 0, 0 };
    do {
        int x = fp_select(nfds, r, w, e, &o);
        if (x != 0) return x;
        co::sleep(16);
        if (ms > 0 && (ms -= 16) < 0) return 0;
    } while (true);
}

unsigned int sleep(unsigned int n) {
    if (!gSched || n == 0) return fp_sleep(n);
    gSched->sleep(n * 1000);
    return 0;
}

int usleep(useconds_t us) {
    if (!gSched || us < 1000) return fp_usleep(us);
    gSched->sleep(us / 1000);
    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
    if (!gSched) return fp_nanosleep(req, rem);

    int ms = (int) (req->tv_sec * 1000 + req->tv_nsec / 1000000);
    if (ms < 1) return fp_nanosleep(req, rem);

    gSched->sleep(ms);
    return 0;
}

#ifdef __linux__
int epoll_wait(int epfd, struct epoll_event* events, int n, int ms) {
    if (!gSched || ms == 0) return fp_epoll_wait(epfd, events, n, ms);

    if (ms > 0) {
        EvRead ev(epfd);
        if (!ev.wait(ms, 0)) return 0; // timeout
        return fp_epoll_wait(epfd, events, n, 0);
    } else {
        gSched->add_ev_read(epfd);
        gSched->yield();
        return fp_epoll_wait(epfd, events, n, 0);
    }
}

int accept4(int fd, struct sockaddr* addr, socklen_t* addrlen, int flags) {
    if (!gSched) return fp_accept4(fd, addr, addrlen, flags);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_accept4(fd, addr, addrlen, flags);

    do {
        int conn_fd = fp_accept4(fd, addr, addrlen, flags);
        if (conn_fd != -1) return conn_fd;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            gSched->add_ev_read(fd);
            gSched->yield();
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int gethostbyname_r(
    const char* name,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    if (!gSched) return fp_gethostbyname_r(name, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex());
    return fp_gethostbyname_r(name, ret, buf, len, res, err);
}

int gethostbyname2_r(
    const char* name, int af,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    if (!gSched) return fp_gethostbyname2_r(name, af, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex());
    return fp_gethostbyname2_r(name, af, ret, buf, len, res, err);
}

int gethostbyaddr_r(
    const void* addr, socklen_t addrlen, int type,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    if (!gSched) return fp_gethostbyaddr_r(addr, addrlen, type, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex());
    return fp_gethostbyaddr_r(addr, addrlen, type, ret, buf, len, res, err);
}

struct hostent* gethostbyname(const char* name) {
    if (!gSched) return fp_gethostbyname(name);
    if (!name) return 0;

    fastream fs(1024);
    struct hostent* ent = gHostEnt();
    struct hostent* res = 0;
    int* err = (int*) fs.data();

    int r = -1;
    while (r = gethostbyname_r(name, ent,
        (char*)(fs.data() + 8), fs.capacity() - 8, &res, err) == ERANGE &&
        *err == NETDB_INTERNAL)
    {
        fs.reserve(fs.capacity() << 1);
        err = (int*) fs.data();
    }

    if (r == 0 && ent == res) return res;
    return 0;
}

struct hostent* gethostbyname2(const char* name, int af) {
    if (!gSched) return fp_gethostbyname2(name, af);
    if (!name) return 0;

    fastream fs(1024);
    struct hostent* ent = gHostEnt();
    struct hostent* res = 0;
    int* err = (int*) fs.data();

    int r = -1;
    while (r = gethostbyname2_r(name, af, ent,
        (char*)(fs.data() + 8), fs.capacity() - 8, &res, err) == ERANGE &&
        *err == NETDB_INTERNAL)
    {
        fs.reserve(fs.capacity() << 1);
        err = (int*) fs.data();
    }

    if (r == 0 && ent == res) return res;
    return 0;
}

struct hostent* gethostbyaddr(const void* addr, socklen_t len, int type) {
    if (!gSched) return fp_gethostbyaddr(addr, len, type);
    if (!addr) return 0;

    fastream fs(1024);
    struct hostent* ent = gHostEnt();
    struct hostent* res = 0;
    int* err = (int*) fs.data();

    int r = -1;
    while (r = gethostbyaddr_r(addr, len, type, ent,
        (char*)(fs.data() + 8), fs.capacity() - 8, &res, err) == ERANGE &&
        *err == NETDB_INTERNAL)
    {
        fs.reserve(fs.capacity() << 1);
        err = (int*) fs.data();
    }

    if (r == 0 && ent == res) return res;
    return 0;
}

#else
int kevent(int kq, const struct kevent* c, int nc, struct kevent* e, int ne, const struct timespec* ts) {
    if (!gSched || c) return fp_kevent(kq, c, nc, e, ne, ts);

    int ms = -1;
    if (ts) ms = ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
    if (ms == 0) return fp_kevent(kq, c, nc, e, ne, ts);

    if (ms > 0) {
        EvRead ev(kq);
        if (!ev.wait(ms, 0)) return 0; // timeout
        return fp_kevent(kq, c, nc, e, ne, 0);
    } else {
        gSched->add_ev_read(kq);
        gSched->yield();
        return fp_kevent(kq, c, nc, e, ne, 0);
    }
}

struct hostent* gethostbyname(const char* name) {
    if (!gSched) return fp_gethostbyname(name);

    co::MutexGuard g(gDnsMtx);
    struct hostent* r = fp_gethostbyname(name);
    if (!r) return 0;

    struct hostent* ent = gHostEnt();
    *ent = *r;
    return ent;
}

struct hostent* gethostbyaddr(const void* addr, socklen_t len, int type) {
    if (!gSched) return fp_gethostbyaddr(addr, len, type);

    co::MutexGuard g(gDnsMtx);
    struct hostent* r = fp_gethostbyaddr(addr, len, type);
    if (!r) return 0;

    struct hostent* ent = gHostEnt();
    *ent = *r;
    return ent;
}
#endif

#undef do_hook
#undef do_hook_ms

} // "C"

namespace co {

bool init_hook();
static bool _dummy = init_hook();

bool init_hook() {
    (void) _dummy;

    fp_connect = (connect_fp_t) dlsym(RTLD_NEXT, "connect");
    fp_accept = (accept_fp_t) dlsym(RTLD_NEXT, "accept");
    fp_close = (close_fp_t) dlsym(RTLD_NEXT, "close");
    fp_shutdown = (shutdown_fp_t) dlsym(RTLD_NEXT, "shutdown");

    fp_read = (read_fp_t) dlsym(RTLD_NEXT, "read");
    fp_readv = (readv_fp_t) dlsym(RTLD_NEXT, "readv");
    fp_recv = (recv_fp_t) dlsym(RTLD_NEXT, "recv");
    fp_recvfrom = (recvfrom_fp_t) dlsym(RTLD_NEXT, "recvfrom");
    fp_recvmsg = (recvmsg_fp_t) dlsym(RTLD_NEXT, "recvmsg");

    fp_write = (write_fp_t) dlsym(RTLD_NEXT, "write");
    fp_writev = (writev_fp_t) dlsym(RTLD_NEXT, "writev");
    fp_send = (send_fp_t) dlsym(RTLD_NEXT, "send");
    fp_sendto = (sendto_fp_t) dlsym(RTLD_NEXT, "sendto");
    fp_sendmsg = (sendmsg_fp_t) dlsym(RTLD_NEXT, "sendmsg");

    fp_poll = (poll_fp_t) dlsym(RTLD_NEXT, "poll");
    fp_select = (select_fp_t) dlsym(RTLD_NEXT, "select");

    fp_sleep = (sleep_fp_t) dlsym(RTLD_NEXT, "sleep");
    fp_usleep = (usleep_fp_t) dlsym(RTLD_NEXT, "usleep");
    fp_nanosleep = (nanosleep_fp_t) dlsym(RTLD_NEXT, "nanosleep");

    fp_gethostbyname = (gethostbyname_fp_t) dlsym(RTLD_NEXT, "gethostbyname");
    fp_gethostbyaddr = (gethostbyaddr_fp_t) dlsym(RTLD_NEXT, "gethostbyaddr");

  #ifdef __linux__
    fp_epoll_wait = (epoll_wait_fp_t) dlsym(RTLD_NEXT, "epoll_wait");
    fp_accept4 = (accept4_fp_t) dlsym(RTLD_NEXT, "accept4");
    fp_gethostbyname2 = (gethostbyname2_fp_t) dlsym(RTLD_NEXT, "gethostbyname2");
    fp_gethostbyname_r = (gethostbyname_r_fp_t) dlsym(RTLD_NEXT, "gethostbyname_r");
    fp_gethostbyname2_r = (gethostbyname2_r_fp_t) dlsym(RTLD_NEXT, "gethostbyname2_r");
    fp_gethostbyaddr_r = (gethostbyaddr_r_fp_t) dlsym(RTLD_NEXT, "gethostbyaddr_r");
  #else
    fp_kevent = (kevent_fp_t) dlsym(RTLD_NEXT, "kevent");
  #endif

    return true;
}

} // co
