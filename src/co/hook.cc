#ifndef _WIN32

#include "co/co.h"
#include <errno.h>
#include <dlfcn.h>
#include <vector>
#include <unordered_map>

namespace co {
using namespace co::xx;

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
        return _t.send_timeout;
    }

    int recv_timeout() const {
        return _t.recv_timeout;
    }

    void set_send_timeout(int ms) {
        _t.send_timeout = ms;
    }

    void set_recv_timeout(int ms) {
        _t.recv_timeout = ms;
    }

  private:
    union {
        struct {
            int32 send_timeout;
            int32 recv_timeout;
        } _t;

        int64 _v;
    };
};

class Hook {
  public:
    Hook() : _hk(co::scheduler_num()) {}
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
                _p->err.append(raw_strerror(e)).append('\0');
            }
            _p->pos[e] = pos;
            return _p->err.data() + pos;
        }
    }

  private:
    thread_ptr<T> _p;
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
    static std::vector<struct hostent> ents(co::scheduler_num());
    return &ents[gSched->id()];
}

#ifdef __linux__
inline co::Mutex& gDnsMutex() {
    static std::vector<co::Mutex> mtx(co::scheduler_num());
    return mtx[gSched->id()];
}
#else
inline co::Mutex& gDnsMutex() {
    static co::Mutex mtx;
    return mtx;
}
#endif


extern "C" {

def_raw_api(strerror);
def_raw_api(close);
def_raw_api(shutdown);
def_raw_api(connect);
def_raw_api(accept);
def_raw_api(read);
def_raw_api(readv);
def_raw_api(recv);
def_raw_api(recvfrom);
def_raw_api(recvmsg);
def_raw_api(write);
def_raw_api(writev);
def_raw_api(send);
def_raw_api(sendto);
def_raw_api(sendmsg);
def_raw_api(poll);
def_raw_api(select);
def_raw_api(sleep);
def_raw_api(usleep);
def_raw_api(nanosleep);
def_raw_api(gethostbyname);
def_raw_api(gethostbyaddr);

#ifdef __linux__
def_raw_api(epoll_wait);
def_raw_api(accept4);
def_raw_api(gethostbyname2);
def_raw_api(gethostbyname_r);
def_raw_api(gethostbyname2_r);
def_raw_api(gethostbyaddr_r);
#else
def_raw_api(kevent);
#endif


#define init_hook(f) \
    if (!raw_api(f)) atomic_set(&raw_api(f), dlsym(RTLD_NEXT, #f))

#define do_hook(f, ev, ms) \
    do { \
        auto r = f; \
        if (r != -1) return r; \
        if (errno == EWOULDBLOCK || errno == EAGAIN) { \
            if (!ev.wait(ms)) return -1; \
        } else if (errno != EINTR) { \
            return -1; \
        } \
    } while (true)


char* strerror(int err) {
    init_hook(strerror);
    static co::Error e;
    return (char*) e.strerror(err);
}

int shutdown(int fd, int how) {
    init_hook(shutdown);
    if (!gSched) return raw_api(shutdown)(fd, how);

    char c = (how == SHUT_RD ? 'r' : (how == SHUT_WR ? 'w' : 'b'));
    auto hi = gHook().on_shutdown(fd, c);
    if (!hi.hookable()) return raw_api(shutdown)(fd, how);
    return co::shutdown(fd, c);
}

int close(int fd) {
    init_hook(close);
    if (!gSched) return raw_api(close)(fd);

    auto hi = gHook().on_close(fd);
    if (!hi.hookable()) return raw_api(close)(fd);
    return co::close(fd);
}

int __close(int fd) {
    return close(fd);
}

/*
 * From man-pages socket(7):  (man 7 socket)
 * SO_RCVTIMEO and SO_SNDTIMEO
 *     Specify the receiving or sending timeouts until reporting an error. The
 *     argument is a struct timeval. If an input or output function blocks for
 *     this period of time, and data has been sent or received, the return value
 *     of that function will be the amount of data transferred; if no data has
 *     been transferred and the timeout has been reached, then -1 is returned
 *     with errno set to EAGAIN or EWOULDBLOCK, or EINPROGRESS (for connect(2))
 *     just as if the socket was specified to be nonblocking.
 */
int connect(int fd, const struct sockaddr* addr, socklen_t addrlen) {
    init_hook(connect);
    if (!gSched) return raw_api(connect)(fd, addr, addrlen);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(connect)(fd, addr, addrlen);

    int r = co::connect(fd, addr, addrlen, hi.send_timeout());
    if (r == -1 && errno == ETIMEDOUT) errno = EINPROGRESS; // set errno to EINPROGRESS

    gHook().erase(fd);
    return r;
}

int accept(int fd, struct sockaddr* addr, socklen_t* addrlen) {
    init_hook(accept);
    if (!gSched) return raw_api(accept)(fd, addr, addrlen);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(accept)(fd, addr, addrlen);

    IoEvent ev(fd, EV_read);
    do {
        int conn_fd = raw_api(accept)(fd, addr, addrlen);
        if (conn_fd != -1) return conn_fd;

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            ev.wait();
        } else if (errno != EINTR) {
            return -1;
        }
    } while (true);
}

ssize_t read(int fd, void* buf, size_t count) {
    init_hook(read);
    if (!gSched) return raw_api(read)(fd, buf, count);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(read)(fd, buf, count);

    IoEvent ev(fd, EV_read);
    do_hook(raw_api(read)(fd, buf, count), ev, hi.recv_timeout());
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
    init_hook(readv);
    if (!gSched) return raw_api(readv)(fd, iov, iovcnt);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(readv)(fd, iov, iovcnt);

    IoEvent ev(fd, EV_read);
    do_hook(raw_api(readv)(fd, iov, iovcnt), ev, hi.recv_timeout());
}

ssize_t recv(int fd, void* buf, size_t len, int flags) {
    init_hook(recv);
    if (!gSched) return raw_api(recv)(fd, buf, len, flags);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(recv)(fd, buf, len, flags);

    IoEvent ev(fd, EV_read);
    do_hook(raw_api(recv)(fd, buf, len, flags), ev, hi.recv_timeout());
}

ssize_t recvfrom(int fd, void* buf, size_t len, int flags, struct sockaddr* addr, socklen_t* addrlen) {
    init_hook(recvfrom);
    if (!gSched) return raw_api(recvfrom)(fd, buf, len, flags, addr, addrlen);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(recvfrom)(fd, buf, len, flags, addr, addrlen);

    IoEvent ev(fd, EV_read);
    do_hook(raw_api(recvfrom)(fd, buf, len, flags, addr, addrlen), ev, hi.recv_timeout());
}

ssize_t recvmsg(int fd, struct msghdr* msg, int flags) {
    init_hook(recvmsg);
    if (!gSched) return raw_api(recvmsg)(fd, msg, flags);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(recvmsg)(fd, msg, flags);

    IoEvent ev(fd, EV_read);
    do_hook(raw_api(recvmsg)(fd, msg, flags), ev, hi.recv_timeout());
}

ssize_t write(int fd, const void* buf, size_t count) {
    init_hook(write);
    if (!gSched) return raw_api(write)(fd, buf, count);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(write)(fd, buf, count);

    IoEvent ev(fd, EV_write);
    do_hook(raw_api(write)(fd, buf, count), ev, hi.send_timeout());
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
    init_hook(writev);
    if (!gSched) return raw_api(writev)(fd, iov, iovcnt);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(writev)(fd, iov, iovcnt);

    IoEvent ev(fd, EV_write);
    do_hook(raw_api(writev)(fd, iov, iovcnt), ev, hi.send_timeout());
}

ssize_t send(int fd, const void* buf, size_t len, int flags) {
    init_hook(send);
    if (!gSched) return raw_api(send)(fd, buf, len, flags);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(send)(fd, buf, len, flags);

    IoEvent ev(fd, EV_write);
    do_hook(raw_api(send)(fd, buf, len, flags), ev, hi.send_timeout());
}

ssize_t sendto(int fd, const void* buf, size_t len, int flags, const struct sockaddr* addr, socklen_t addrlen) {
    init_hook(sendto);
    if (!gSched) return raw_api(sendto)(fd, buf, len, flags, addr, addrlen);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(sendto)(fd, buf, len, flags, addr, addrlen);

    IoEvent ev(fd, EV_write);
    do_hook(raw_api(sendto)(fd, buf, len, flags, addr, addrlen), ev, hi.send_timeout());
}

ssize_t sendmsg(int fd, const struct msghdr* msg, int flags) {
    init_hook(sendmsg);
    if (!gSched) return raw_api(sendmsg)(fd, msg, flags);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(sendmsg)(fd, msg, flags);

    IoEvent ev(fd, EV_write);
    do_hook(raw_api(sendmsg)(fd, msg, flags), ev, hi.send_timeout());
}

int poll(struct pollfd* fds, nfds_t nfds, int ms) {
    init_hook(poll);
    if (!gSched || ms == 0) return raw_api(poll)(fds, nfds, ms);

    do {
        if (nfds == 1) {
            int fd = fds[0].fd;
            if (fds[0].events == POLLIN) {
                if (!gSched->add_io_event(fd, EV_read)) break;
            } else if (fds[0].events == POLLOUT) {
                if (!gSched->add_io_event(fd, EV_write)) break;
            } else {
                break;
            }

            if (ms > 0) gSched->add_timer(ms);
            gSched->yield();
            gSched->del_io_event(fd);
            if (ms > 0 && gSched->timeout()) return 0;

            fds[0].revents = fds[0].events;
            return 1;
        }
    } while (0);

    // it's boring to hook poll when nfds > 1, just check poll every 16 ms
    do {
        int r = raw_api(poll)(fds, nfds, 0);
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
    if (!gSched) return raw_api(select)(nfds, r, w, e, tv);

    int ms = -1;
    if (tv) ms = (int) (tv->tv_sec * 1000 + tv->tv_usec / 1000);
    if (ms == 0) return raw_api(select)(nfds, r, w, e, tv);

    if ((nfds == 0 || (!r && !w && !e)) && ms > 0) {
        co::sleep(ms);
        return 0;
    }

    // it's boring to hook select, just check select every 16 ms
    struct timeval o = { 0, 0 };
    do {
        int x = raw_api(select)(nfds, r, w, e, &o);
        if (x != 0) return x;
        co::sleep(16);
        if (ms > 0 && (ms -= 16) < 0) return 0;
    } while (true);
}

unsigned int sleep(unsigned int n) {
    init_hook(sleep);
    if (!gSched || n == 0) return raw_api(sleep)(n);
    gSched->sleep(n * 1000);
    return 0;
}

int usleep(useconds_t us) {
    init_hook(usleep);
    if (!gSched || us < 1000) return raw_api(usleep)(us);
    gSched->sleep(us / 1000);
    return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
    init_hook(nanosleep);
    if (!gSched) return raw_api(nanosleep)(req, rem);

    int ms = (int) (req->tv_sec * 1000 + req->tv_nsec / 1000000);
    if (ms < 1) return raw_api(nanosleep)(req, rem);

    gSched->sleep(ms);
    return 0;
}

#ifdef __linux__
int epoll_wait(int epfd, struct epoll_event* events, int n, int ms) {
    init_hook(epoll_wait);
    if (!gSched || ms == 0) return raw_api(epoll_wait)(epfd, events, n, ms);

    IoEvent ev(epfd, EV_read);
    if (!ev.wait(ms)) return 0; // timeout
    return raw_api(epoll_wait)(epfd, events, n, 0);
}

int accept4(int fd, struct sockaddr* addr, socklen_t* addrlen, int flags) {
    init_hook(accept4);
    if (!gSched) return raw_api(accept4)(fd, addr, addrlen, flags);

    auto hi = gHook().get_hook_info(fd);
    if (!hi.hookable()) return raw_api(accept4)(fd, addr, addrlen, flags);

    IoEvent ev(fd, EV_read);
    do {
        int conn_fd = raw_api(accept4)(fd, addr, addrlen, flags);
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
    if (!gSched) return raw_api(gethostbyname_r)(name, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex());
    return raw_api(gethostbyname_r)(name, ret, buf, len, res, err);
}

int gethostbyname2_r(
    const char* name, int af,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    init_hook(gethostbyname2_r);
    if (!gSched) return raw_api(gethostbyname2_r)(name, af, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex());
    return raw_api(gethostbyname2_r)(name, af, ret, buf, len, res, err);
}

int gethostbyaddr_r(
    const void* addr, socklen_t addrlen, int type,
    struct hostent* ret, char* buf, size_t len,
    struct hostent** res, int* err)
{
    init_hook(gethostbyaddr_r);
    if (!gSched) return raw_api(gethostbyaddr_r)(addr, addrlen, type, ret, buf, len, res, err);
    co::MutexGuard g(gDnsMutex());
    return raw_api(gethostbyaddr_r)(addr, addrlen, type, ret, buf, len, res, err);
}

struct hostent* gethostbyname(const char* name) {
    init_hook(gethostbyname);
    if (!gSched) return raw_api(gethostbyname)(name);
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
    if (!gSched) return raw_api(gethostbyname2)(name, af);
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
    if (!gSched) return raw_api(gethostbyaddr)(addr, len, type);
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
    if (!gSched || c) return raw_api(kevent)(kq, c, nc, e, ne, ts);

    int ms = -1;
    if (ts) ms = ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
    if (ms == 0) return raw_api(kevent)(kq, c, nc, e, ne, ts);

    IoEvent ev(kq, EV_read);
    if (!ev.wait(ms)) return 0; // timeout
    return raw_api(kevent)(kq, c, nc, e, ne, 0);
}

struct hostent* gethostbyname(const char* name) {
    init_hook(gethostbyname);
    if (!gSched) return raw_api(gethostbyname)(name);

    co::MutexGuard g(gDnsMutex());
    struct hostent* r = raw_api(gethostbyname)(name);
    if (!r) return 0;

    struct hostent* ent = gHostEnt();
    *ent = *r;
    return ent;
}

struct hostent* gethostbyaddr(const void* addr, socklen_t len, int type) {
    init_hook(gethostbyaddr);
    if (!gSched) return raw_api(gethostbyaddr)(addr, len, type);

    co::MutexGuard g(gDnsMutex());
    struct hostent* r = raw_api(gethostbyaddr)(addr, len, type);
    if (!r) return 0;

    struct hostent* ent = gHostEnt();
    *ent = *r;
    return ent;
}
#endif

static bool init_hooks();
static bool _dummy = init_hooks();

static bool init_hooks() {
    (void) _dummy;
    init_hook(strerror);
    init_hook(close);
    init_hook(shutdown);
    init_hook(connect);
    init_hook(accept);
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

    (void) gHook();
    return true;
}

} // "C"

#undef do_hook
#undef init_hook

#endif
