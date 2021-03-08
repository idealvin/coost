#pragma once

#include "def.h"
#include "closure.h"
#include "byte_order.h"
#include "fastring.h"

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

// Add a task, which will run as a coroutine.
// Supported function types:
//   void f();
//   void f(void*);          // func with a param
//   void T::f();            // method in a class
//   std::function<void()>;
void go(Closure* cb);

inline void go(void (*f)()) {
    go(new_closure(f));
}

inline void go(void (*f)(void*), void* p) {
    go(new_closure(f, p));
}

template<typename T>
inline void go(void (T::*f)(), T* p) {
    go(new_closure(f, p));
}

// !! Performance of std::function is poor. Try to avoid it.
inline void go(const std::function<void()>& f) {
    go(new_closure(f));
}

inline void go(std::function<void()>&& f) {
    go(new_closure(std::move(f)));
}

void sleep(unsigned int ms);

// stop coroutine schedulers
void stop();

// max number of schedulers. It is os::cpunum() right now.
// scheduler id is from 0 to max_sched_num-1.
int max_sched_num();

// id of the current scheduler, -1 for non-scheduler
int sched_id();

// id of the current coroutine, -1 for non-coroutine
int coroutine_id();

// co::Event is for communications between coroutines.
// It's similar to SyncEvent for threads.
class Event {
  public:
    Event();
    ~Event();

    Event(Event&& e) : _p(e._p) { e._p = 0; }

    Event(const Event&) = delete;
    void operator=(const Event&) = delete;

    // MUST be called in coroutine
    void wait();

    // return false if timeout
    // MUST be called in coroutine
    bool wait(unsigned int ms);

    // wakeup all waiting coroutines
    // can be called from anywhere
    void signal();

  private:
    void* _p;
};

// co::Mutex is a mutex lock for coroutines.
// It's similar to Mutex for threads.
class Mutex {
  public:
    Mutex();
    ~Mutex();

    Mutex(Mutex&& m) : _p(m._p) { m._p = 0; }

    Mutex(const Mutex&) = delete;
    void operator=(const Mutex&) = delete;

    // MUST be called in coroutine
    void lock();

    // can be called from anywhere
    void unlock();

    // can be called from anywhere
    bool try_lock();

  private:
    void* _p;
};

class MutexGuard {
  public:
    explicit MutexGuard(co::Mutex& lock) : _lock(lock) {
        _lock.lock();
    }

    explicit MutexGuard(co::Mutex* lock) : _lock(*lock) {
        _lock.lock();
    }

    MutexGuard(const MutexGuard&) = delete;
    void operator=(const MutexGuard&) = delete;

    ~MutexGuard() {
        _lock.unlock();
    }

  private:
    co::Mutex& _lock;
};

// co::Pool is a general pool for coroutines.
// It stores void* pointers internally.
// It is coroutine-safe. Each thread has its own pool.
class Pool {
  public:
    Pool();
    ~Pool();

    // @ccb:  a create callback       []() { return (void*) new T; }
    //   when pop from an empty pool, this callback is used to create an element
    //
    // @dcb:  a destroy callback      [](void* p) { delete (T*)p; }
    //   this callback is used to destroy an element when needed
    //
    // @cap:  max capacity of the pool for each thread
    //   this argument is ignored if the destory callback is not set
    Pool(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap=(size_t)-1);

    Pool(Pool&& p) : _p(p._p) { p._p = 0; }

    Pool(const Pool&) = delete;
    void operator=(const Pool&) = delete;

    // pop an element from the pool
    // return NULL if the pool is empty and the create callback is not set
    // MUST be called in coroutine
    void* pop();

    // push an element to the pool
    // nothing is done if p is NULL
    // MUST be called in coroutine
    void push(void* p);

  private:
    void* _p;
};

// pop an element from co::Pool in constructor, and push it back in destructor.
// The element is stored internally as a pointer of type T*.
// As owner of the pointer, its behavior is similar to std::unique_ptr.
template<typename T>
class PoolGuard {
  public:
    explicit PoolGuard(Pool& pool) : _pool(pool) {
        _p = (T*) _pool.pop();
    }

    explicit PoolGuard(Pool* pool) : _pool(*pool) {
        _p = (T*) _pool.pop();
    }

    PoolGuard(const PoolGuard&) = delete;
    void operator=(const PoolGuard&) = delete;

    ~PoolGuard() {
        _pool.push(_p);
    }

    T* get() const {
        return _p;
    }

    void reset(T* p = 0) {
        if (_p != p) { delete _p; _p = p; }
    }

    void operator=(T* p) {
        this->reset(p);
    }

    T* operator->() const {
        return _p;
    }

    bool operator==(T* p) const {
        return _p == p;
    }

    bool operator!=(T* p) const {
        return _p != p;
    }

  private:
    Pool& _pool;
    T* _p;
};

// return a non-blocking socket on Linux & Mac, an overlapped socket on windows
// @domain: address family, AF_INET, AF_INET6, etc.
// @type:   socket type, SOCK_STREAM, SOCK_DGRAM, etc.
// @proto:  protocol, IPPROTO_TCP, IPPROTO_UDP, etc.
sock_t socket(int domain, int type, int proto);

// @af: address family, AF_INET, AF_INET6, etc.
inline sock_t tcp_socket(int af=AF_INET) {
    return co::socket(af, SOCK_STREAM, IPPROTO_TCP);
}

// @af: address family, AF_INET, AF_INET6, etc.
inline sock_t udp_socket(int af=AF_INET) {
    return co::socket(af, SOCK_DGRAM, IPPROTO_UDP);
}

// close the fd @ms milliseconds later
int close(sock_t fd, int ms=0);

// @c:  'r' for SHUT_RD, 'w' for SHUT_WR, 'b' for SHUT_RDWR
int shutdown(sock_t fd, char c='b');

int bind(sock_t fd, const void* addr, int addrlen);

int listen(sock_t fd, int backlog);

// return a non-blocking socket on Linux & Mac, an overlapped socket on windows
sock_t accept(sock_t fd, void* addr, int* addrlen);

// connect until connection is done or timeout in @ms, or any error occured
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

} // namespace co

using co::go;
