#ifndef _WIN32

#include "hook.h"
#include "scheduler.h"
#include "io_event.h"
#include <dlfcn.h>

namespace co {

class HookInfo {
  public:
    HookInfo() : HookInfo(0) {}
    explicit HookInfo(int64 v) : _v(v) {}
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

    HookInfo on_close(int fd) {
        auto& hk = _hk[gSched->id()];
        auto it = hk.find(fd);
        if (it == hk.end()) return HookInfo();

        HookInfo hi = it->second;
        hk.erase(it);
        return hi;
    }

    HookInfo on_shutdown(int fd, char c) {
        auto& hk = _hk[gSched->id()];
        auto it = hk.find(fd);
        if (it == hk.end()) return HookInfo();

        HookInfo res = it->second;
        HookInfo& hi = it->second;
        if (c == 'r') {
            hi.set_recv_timeout(0);
            if (!hi.hookable()) hk.erase(it);
        } else if (c == 'w') {
            hi.set_send_timeout(0);
            if (!hi.hookable()) hk.erase(it);
        } else {
            hk.erase(it);
        }

        return res;
    }

    HookInfo get_hook_info(int fd) {
        auto& hk = _hk[gSched->id()];
        auto it = hk.find(fd);
        if (it != hk.end()) return it->second;

        // check whether @fd is non-block, as we don't need to hook non-block socket
        int flag = fcntl(fd, F_GETFL);
        if (flag & O_NONBLOCK) return hk[fd] = HookInfo();

        // check whether recv/send timeout is set for @fd
        int recv_timeout = this->_Get_timeout(fd, 'r');
        int send_timeout = this->_Get_timeout(fd, 'w');
        if (recv_timeout == 0 || send_timeout == 0) return hk[fd] = HookInfo();

        HookInfo hi;
        hi.set_recv_timeout(recv_timeout);
        hi.set_send_timeout(send_timeout);

        fcntl(fd, F_SETFL, flag | O_NONBLOCK);
        return hk[fd] = hi;
    }

  private:
    std::vector<std::unordered_map<int, HookInfo>> _hk;

    // return 0 if @fd is not valid, or it does not refer to a socket.
    // return -1 if timeout is not set for @fd, otherwise return a positive value.
    int _Get_timeout(int fd, char c) {
        struct timeval tv;
        int len = sizeof(tv);
        int opt = (c == 'r' ? SO_RCVTIMEO : SO_SNDTIMEO);
        int r = co::getsockopt(fd, SOL_SOCKET, opt, &tv, &len);
        if (r == 0) {
            int ms = (int) (tv.tv_sec * 1000 + tv.tv_usec / 1000);
            return ms > 0 ? ms : -1;
        }
        return 0;
    }
};

} // co

inline co::Hook& gHook() {
    static co::Hook hook;
    return hook;
}

using co::gSched;
using co::EV_read;
using co::EV_write;
using co::IoEvent;

inline struct hostent* gHostEnt() {
    static std::vector<struct hostent> ents(FLG_co_sched_num);
    return &ents[gSched->id()];
}

#ifdef __linux__
inline co::Mutex& gDnsMutex() {
    static std::vector<co::Mutex> mtx(FLG_co_sched_num);
    return mtx[gSched->id()];
}
#else
inline co::Mutex& gDnsMutex() {
    static co::Mutex mtx;
    return mtx;
}
#endif


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


#define init_hook(f) \
    if (!fp_##f) atomic_set(&fp_##f, dlsym(RTLD_NEXT, #f))

#define do_hook(f, ev, ms) \
    do { \
        auto r = f; \
        if (r != -1) return r; \
        if (errno == EWOULDBLOCK || errno == EAGAIN) { \
            if (!ev.wait(ms, false)) return -1; \
        } else if (errno != EINTR) { \
            return -1; \
        } \
    } while (true)


int connect(int fd, const struct sockaddr* addr, socklen_t addrlen) {
    init_hook(connect);
    if (!gSched) return fp_connect(fd, addr, addrlen);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_connect(fd, addr, addrlen);

    int r;
    r = co::connect(fd, addr, addrlen, hi.send_timeout());
    // set errno to EINPROGRESS if SO_SNDTIMEO is set and the timeout expires
    if (r == -1 && errno == ETIMEDOUT) errno = EINPROGRESS;

    gHook().erase(fd);
    return r;
}

int accept(int fd, struct sockaddr* addr, socklen_t* addrlen) {
    init_hook(accept);
    if (!gSched) return fp_accept(fd, addr, addrlen);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_accept(fd, addr, addrlen);

    IoEvent ev(fd, EV_read);
    do {
        int conn_fd = fp_accept(fd, addr, addrlen);
        if (conn_fd != -1) return conn_fd;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            ev.wait();
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

int shutdown(int fd, int how) {
    init_hook(shutdown);
    if (!gSched) return fp_shutdown(fd, how);

    char c = (how == SHUT_RD ? 'r' : (how == SHUT_WR ? 'w' : 'b'));
    auto hi = gHook().on_shutdown(fd, c);
    if (!hi.hookable()) return fp_shutdown(fd, how);
    return co::shutdown(fd, c);
}

int close(int fd) {
    init_hook(close);
    if (!gSched) return fp_close(fd);

    auto hi = gHook().on_close(fd);
    if (!hi.hookable()) return fp_close(fd);
    return co::close(fd);
}

int __close(int fd) {
    return close(fd);
}

ssize_t read(int fd, void* buf, size_t count) {
    init_hook(read);
    if (!gSched) return fp_read(fd, buf, count);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_read(fd, buf, count);

    IoEvent ev(fd, EV_read);
    do_hook(fp_read(fd, buf, count), ev, hi.recv_timeout());
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
    init_hook(readv);
    if (!gSched) return fp_readv(fd, iov, iovcnt);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_readv(fd, iov, iovcnt);

    IoEvent ev(fd, EV_read);
    do_hook(fp_readv(fd, iov, iovcnt), ev, hi.recv_timeout());
}

ssize_t recv(int fd, void* buf, size_t len, int flags) {
    init_hook(recv);
    if (!gSched) return fp_recv(fd, buf, len, flags);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_recv(fd, buf, len, flags);

    IoEvent ev(fd, EV_read);
    do_hook(fp_recv(fd, buf, len, flags), ev, hi.recv_timeout());
}

ssize_t recvfrom(int fd, void* buf, size_t len, int flags, struct sockaddr* addr, socklen_t* addrlen) {
    init_hook(recvfrom);
    if (!gSched) return fp_recvfrom(fd, buf, len, flags, addr, addrlen);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_recvfrom(fd, buf, len, flags, addr, addrlen);

    IoEvent ev(fd, EV_read);
    do_hook(fp_recvfrom(fd, buf, len, flags, addr, addrlen), ev, hi.recv_timeout());
}

ssize_t recvmsg(int fd, struct msghdr* msg, int flags) {
    init_hook(recvmsg);
    if (!gSched) return fp_recvmsg(fd, msg, flags);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_recvmsg(fd, msg, flags);

    IoEvent ev(fd, EV_read);
    do_hook(fp_recvmsg(fd, msg, flags), ev, hi.recv_timeout());
}

ssize_t write(int fd, const void* buf, size_t count) {
    init_hook(write);
    if (!gSched) return fp_write(fd, buf, count);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_write(fd, buf, count);

    IoEvent ev(fd, EV_write);
    do_hook(fp_write(fd, buf, count), ev, hi.send_timeout());
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    init_hook(writev);
    if (!gSched) return fp_writev(fd, iov, iovcnt);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_writev(fd, iov, iovcnt);

    IoEvent ev(fd, EV_write);
    do_hook(fp_writev(fd, iov, iovcnt), ev, hi.send_timeout());
}

ssize_t send(int fd, const void* buf, size_t len, int flags) {
    init_hook(send);
    if (!gSched) return fp_send(fd, buf, len, flags);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_send(fd, buf, len, flags);

    IoEvent ev(fd, EV_write);
    do_hook(fp_send(fd, buf, len, flags), ev, hi.send_timeout());
}

ssize_t sendto(int fd, const void* buf, size_t len, int flags, const struct sockaddr* addr, socklen_t addrlen) {
    init_hook(sendto);
    if (!gSched) return fp_sendto(fd, buf, len, flags, addr, addrlen);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_sendto(fd, buf, len, flags, addr, addrlen);

    IoEvent ev(fd, EV_write);
    do_hook(fp_sendto(fd, buf, len, flags, addr, addrlen), ev, hi.send_timeout());
}

ssize_t sendmsg(int fd, const struct msghdr* msg, int flags) {
    init_hook(sendmsg);
    if (!gSched) return fp_sendmsg(fd, msg, flags);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_sendmsg(fd, msg, flags);

    IoEvent ev(fd, EV_write);
    do_hook(fp_sendmsg(fd, msg, flags), ev, hi.send_timeout());
}

int poll(struct pollfd* fds, nfds_t nfds, int ms) {
    init_hook(poll);
    if (!gSched || ms == 0) return fp_poll(fds, nfds, ms);

    do {
        if (nfds == 1) {
            int fd = fds[0].fd;
            if (fds[0].events == POLLIN) {
                if (!gSched->add_event(fd, EV_read)) break;
            } else if (fds[0].events == POLLOUT) {
                if (!gSched->add_event(fd, EV_write)) break;
            } else {
                break;
            }

            co::timer_id_t id = co::null_timer_id;
            if (ms > 0) id = gSched->add_timer(ms, false);

            gSched->yield();
            gSched->del_event(fd);
            if (ms > 0 && gSched->timeout()) return 0;

            if (id != co::null_timer_id) gSched->del_timer(id);
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
    init_hook(select);
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
    init_hook(sleep);
    if (!gSched || n == 0) return fp_sleep(n);
    gSched->sleep(n * 1000);
    return 0;
}

int usleep(useconds_t us) {
    init_hook(usleep);
    if (!gSched || us < 1000) return fp_usleep(us);
    gSched->sleep(us / 1000);
    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
    init_hook(nanosleep);
    if (!gSched) return fp_nanosleep(req, rem);

    int ms = (int) (req->tv_sec * 1000 + req->tv_nsec / 1000000);
    if (ms < 1) return fp_nanosleep(req, rem);

    gSched->sleep(ms);
    return 0;
}

#ifdef __linux__
int epoll_wait(int epfd, struct epoll_event* events, int n, int ms) {
    init_hook(epoll_wait);
    if (!gSched || ms == 0) return fp_epoll_wait(epfd, events, n, ms);

    IoEvent ev(epfd, EV_read);
    if (!ev.wait(ms, false)) return 0; // timeout
    return fp_epoll_wait(epfd, events, n, 0);
}

int accept4(int fd, struct sockaddr* addr, socklen_t* addrlen, int flags) {
    init_hook(accept4);
    if (!gSched) return fp_accept4(fd, addr, addrlen, flags);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return fp_accept4(fd, addr, addrlen, flags);

    IoEvent ev(fd, EV_read);
    do {
        int conn_fd = fp_accept4(fd, addr, addrlen, flags);
        if (conn_fd != -1) return conn_fd;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            ev.wait();
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
    init_hook(gethostbyname_r);
    if (!gSched) return fp_gethostbyname_r(name, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex());
    return fp_gethostbyname_r(name, ret, buf, len, res, err);
}

int gethostbyname2_r(
    const char* name, int af,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    init_hook(gethostbyname2_r);
    if (!gSched) return fp_gethostbyname2_r(name, af, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex());
    return fp_gethostbyname2_r(name, af, ret, buf, len, res, err);
}

int gethostbyaddr_r(
    const void* addr, socklen_t addrlen, int type,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    init_hook(gethostbyaddr_r);
    if (!gSched) return fp_gethostbyaddr_r(addr, addrlen, type, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex());
    return fp_gethostbyaddr_r(addr, addrlen, type, ret, buf, len, res, err);
}

struct hostent* gethostbyname(const char* name) {
    init_hook(gethostbyname);
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
    init_hook(gethostbyname2);
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
    init_hook(gethostbyaddr);
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
    init_hook(kevent);
    if (!gSched || c) return fp_kevent(kq, c, nc, e, ne, ts);

    int ms = -1;
    if (ts) ms = ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
    if (ms == 0) return fp_kevent(kq, c, nc, e, ne, ts);

    IoEvent ev(kq, EV_read);
    if (!ev.wait(ms, false)) return 0; // timeout
    return fp_kevent(kq, c, nc, e, ne, 0);
}

struct hostent* gethostbyname(const char* name) {
    init_hook(gethostbyname);
    if (!gSched) return fp_gethostbyname(name);

    co::MutexGuard g(gDnsMutex());
    struct hostent* r = fp_gethostbyname(name);
    if (!r) return 0;

    struct hostent* ent = gHostEnt();
    *ent = *r;
    return ent;
}

struct hostent* gethostbyaddr(const void* addr, socklen_t len, int type) {
    init_hook(gethostbyaddr);
    if (!gSched) return fp_gethostbyaddr(addr, len, type);

    co::MutexGuard g(gDnsMutex());
    struct hostent* r = fp_gethostbyaddr(addr, len, type);
    if (!r) return 0;

    struct hostent* ent = gHostEnt();
    *ent = *r;
    return ent;
}
#endif

} // "C"

namespace co {

bool init_hooks();
static bool _dummy = init_hooks();

bool init_hooks() {
    (void) _dummy;
    init_hook(connect);
    init_hook(accept);
    init_hook(close);
    init_hook(shutdown);
    init_hook(read);
    init_hook(readv);
    init_hook(recv);
    init_hook(recvfrom);
    init_hook(recvmsg);
    init_hook(write);
    init_hook(writev);
    init_hook(send);
    init_hook(sendto);
    init_hook(sendmsg);
    init_hook(poll);
    init_hook(select);
    init_hook(sleep);
    init_hook(usleep);
    init_hook(nanosleep);
    init_hook(gethostbyname);
    init_hook(gethostbyaddr);

  #ifdef __linux__
    init_hook(epoll_wait);
    init_hook(accept4);
    init_hook(gethostbyname2);
    init_hook(gethostbyname_r);
    init_hook(gethostbyname2_r);
    init_hook(gethostbyaddr_r);
  #else
    init_hook(kevent);
  #endif

    return true;
}

#undef do_hook
#undef init_hook

} // co

#endif
