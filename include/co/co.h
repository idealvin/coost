#pragma once

#include "def.h"
#include "closure.h"
#include "fastring.h"
#include "byte_order.h"
#include <vector>

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

/**
 * add a task, which will run as a coroutine 
 *   - It is thread-safe and can be called from anywhere. 
 *   - Closure created by new_closure() will delete itself after Closure::run() 
 *     is done. Users MUST NOT delete it manually.
 *   - Closure is an abstract base class, users are free to implement their own 
 *     subtype of Closure. This may be useful if users do not want a Closure to 
 *     delete itself. See details in co/closure.h.
 * 
 * @param cb  a pointer to a Closure created by new_closure(), or an user-defined Closure.
 */
void go(Closure* cb);

/**
 * add a task, which will run as a coroutine 
 *   - eg.
 *     go(f);               // void f(); 
 *     go([]() { ... });    // lambda 
 *     go(std::bind(...));  // std::bind 
 * 
 *     std::function<void()> x(std::bind(...)); 
 *     go(x);               // std::function<void()> 
 *     go(&x);              // std::function<void()>* 
 *
 *   - If f is a pointer to std::function<void()>, users MUST ensure that the 
 *     object f points to is valid when Closure::run() is running. 
 * 
 * @param f  any runnable object, as long as we can call f() or (*f)().
 */
template<typename F>
inline void go(F&& f) {
    go(new_closure(std::forward<F>(f)));
}

/**
 * add a task, which will run as a coroutine 
 *   - eg.
 *     go(f, 8);   // void f(int);
 *     go(f, p);   // void f(void*);   void* p;
 *     go(f, o);   // void (T::*f)();  T* o;
 * 
 *     std::function<void(P)> x(std::bind(...));
 *     go(x, p);   // P p;
 *     go(&x, p);  // P p; 
 *
 *   - If f is a pointer to std::function<void(P)>, users MUST ensure that the 
 *     object f points to is valid when Closure::run() is running. 
 *
 * @param f  any runnable object, as long as we can call f(p), (*f)(p) or (p->*f)().
 * @param p  parameter of f, or a pointer to an object of class P if f is a method.
 */
template<typename F, typename P>
inline void go(F&& f, P&& p) {
    go(new_closure(std::forward<F>(f), std::forward<P>(p)));
}

/**
 * add a task, which will run as a coroutine 
 *   - eg.
 *     go(f, o, p);   // void (T::*f)(P);  T* o;  P p;
 
 * @param f  a pointer to a method with a parameter in class T.
 * @param t  a pointer to an object of class T.
 * @param p  parameter of f.
 */
template<typename F, typename T, typename P>
inline void go(F&& f, T* t, P&& p) {
    go(new_closure(std::forward<F>(f), t, std::forward<P>(p)));
}

class Scheduler {
  public:
    void go(Closure* cb);

    template<typename F>
    inline void go(F&& f) {
        this->go(new_closure(std::forward<F>(f)));
    }

    template<typename F, typename P>
    inline void go(F&& f, P&& p) {
        this->go(new_closure(std::forward<F>(f), std::forward<P>(p)));
    }

    template<typename F, typename T, typename P>
    inline void go(F&& f, T* t, P&& p) {
        this->go(new_closure(std::forward<F>(f), t, std::forward<P>(p)));
    }

  protected:
    Scheduler() = default;
    ~Scheduler() = default;
};

/**
 * get all schedulers 
 *   
 * @return  a reference of an array, which stores pointers to all the Schedulers
 */
const std::vector<Scheduler*>& all_schedulers();

/**
 * get the current scheduler
 * 
 * @return a pointer to the current scheduler, or NULL if called from a non-scheduler thread.
 */
Scheduler* scheduler();

/**
 * get next scheduler 
 *   - It is useful when users want to create coroutines in the same scheduler. 
 *   - eg. 
 *     auto s = co::next_scheduler();
 *     s->go(f);     // void f();
 *     s->go(g, 7);  // void g(int);
 * 
 * @return a non-null pointer.
 */
Scheduler* next_scheduler();

/**
 * get number of schedulers 
 *   - scheduler id is from 0 to scheduler_num() - 1. 
 *   - This function may be used to implement scheduler-local storage:  
 *                std::vector<T> xx(co::scheduler_num());  
 *     xx[co::scheduler_id()] can be used in a coroutine to access the storage for 
 *     the current scheduler thread.
 * 
 * @return  total number of the schedulers.
 */
int scheduler_num();

/**
 * get id of the current scheduler 
 *   - It is EXPECTED to be called in a coroutine. 
 * 
 * @return  a non-negative id of the current scheduler, or -1 if the current thread 
 *          is not a scheduler thread.
 */
int scheduler_id();

/**
 * get id of the current coroutine 
 *   - It is EXPECTED to be called in a coroutine. 
 *   - Each cocoutine has a unique id. 
 * 
 * @return  a non-negative id of the current coroutine, or -1 if the current thread 
 *          is not a scheduler thread.
 */
int coroutine_id();

/**
 * add a timer for the current coroutine 
 *   - It MUST be called in a coroutine.
 *   - Users MUST call yield() to suspend the coroutine after a timer was added.
 *     When the timer expires, the scheduler will resume the coroutine.
 *
 * @param ms  timeout in milliseconds.
 */
void add_timer(uint32 ms);

enum io_event_t {
    ev_read = 1,
    ev_write = 2,
};

/**
 * add an IO event on a socket to the epoll 
 *   - It MUST be called in a coroutine.
 *   - Users MUST call yield() to suspend the coroutine after an event was added.
 *     When the event is present, the scheduler will resume the coroutine.
 *
 * @param fd  the socket.
 * @param ev  an IO event, either ev_read or ev_write.
 *
 * @return    true on success, false on error.
 */
bool add_io_event(sock_t fd, io_event_t ev);

/**
 * delete an IO event from epoll
 *   - It MUST be called in a coroutine.
 */
void del_io_event(sock_t fd, io_event_t ev);

/**
 * remove all events on the socket 
 *   - It MUST be called in a coroutine.
 */
void del_io_event(sock_t fd);

/**
 * suspend the current coroutine 
 *   - It MUST be called in a coroutine. 
 *   - Usually, users should add an IO event, or a timer, or both in a coroutine, 
 *     and then call yield() to suspend the coroutine. When the event is present 
 *     or the timer expires, the scheduler will resume the coroutine. 
 */
void yield();

/**
 * sleep for milliseconds 
 *   - It is EXPECTED to be called in a coroutine. 
 * 
 * @param ms  time in milliseconds
 */
void sleep(uint32 ms);

/**
 * check whether the current coroutine has timed out 
 *   - It MUST be called in a coroutine.
 *   - When a coroutine returns from an API with a timeout like co::recv, users may 
 *     call co::timeout() to check whether the previous API call has timed out. 
 * 
 * @return  true if timed out, otherwise false.
 */
bool timeout();

/**
 * check whether a pointer is on the stack of the current coroutine 
 *   - It MUST be called in a coroutine. 
 */
bool on_stack(const void* p);

/**
 * stop all coroutine schedulers 
 *   - It is safe to call stop() from anywhere. 
 */
void stop();

/**
 * co::Event is for communications between coroutines
 *   - It is similar to SyncEvent for threads.
 *   - Users SHOULD use co::Event in coroutine environments only.
 */
class Event {
  public:
    Event();
    ~Event();

    Event(Event&& e) : _p(e._p) { e._p = 0; }

    /**
     * wait for a signal
     *   - It MUST be called in a coroutine.
     *   - It blocks until a signal was present.
     */
    void wait();

    /**
     * wait for a signal with a timeout
     *   - It MUST be called in a coroutine.
     *   - It blocks until a signal was present or timeout.
     *
     * @param ms  timeout in milliseconds
     *
     * @return    true if a signal was present before timeout, otherwise false
     */
    bool wait(uint32 ms);

    /**
     * generate a signal on this event
     *   - It is not necessary to call signal() in a coroutine, though usually it is.
     *   - When a signal was present, all the waiting coroutines will be waken up.
     */
    void signal();

  private:
    void* _p;
    DISALLOW_COPY_AND_ASSIGN(Event);
};

/**
 * co::Mutex is a mutex lock for coroutines
 *   - It is similar to Mutex for threads.
 *   - Users SHOULD use co::Mutex in coroutine environments only.
 */
class Mutex {
  public:
    Mutex();
    ~Mutex();

    Mutex(Mutex&& m) : _p(m._p) { m._p = 0; }

    /**
     * acquire the lock
     *   - It MUST be called in a coroutine.
     *   - It will block until the lock was acquired by the calling coroutine.
     */
    void lock();

    /**
     * release the lock
     *   - It SHOULD be called in the coroutine that holds the lock.
     */
    void unlock();

    /**
     * try to acquire the lock
     *   - It SHOULD be called in a coroutine.
     *   - If no coroutine holds the lock, the calling coroutine will get the lock.
     *
     * @return  true if the lock was acquired by the calling coroutine, otherwise false
     */
    bool try_lock();

  private:
    void* _p;
    DISALLOW_COPY_AND_ASSIGN(Mutex);
};

/**
 * guard to release the mutex lock
 *   - lock() is called in the constructor.
 *   - unlock() is called in the destructor.
 */
class MutexGuard {
  public:
    explicit MutexGuard(co::Mutex& lock) : _lock(lock) {
        _lock.lock();
    }

    explicit MutexGuard(co::Mutex* lock) : _lock(*lock) {
        _lock.lock();
    }

    ~MutexGuard() {
        _lock.unlock();
    }

  private:
    co::Mutex& _lock;
    DISALLOW_COPY_AND_ASSIGN(MutexGuard);
};

/**
 * a general pool for coroutine programming
 *   - Pool is designed to be coroutine-safe, users do not need to lock it.
 *   - Each thread holds its own pool, users SHOULD call pop() and push() in the
 *     same thread.
 *   - It stores void* pointers internally and it does not care about the actual
 *     type of the pointer.
 *   - It is usually used as a connection pool in network programming.
 */
class Pool {
  public:
    /**
     * the default constructor
     *   - In this case, ccb and dcb will be NULL.
     */
    Pool();
    ~Pool();

    /**
     * the constructor with parameters
     *
     * @param ccb  a create callback like:   []() { return (void*) new T; }
     *             it is used to create an element when pop from an empty pool.
     * @param dcb  a destroy callback like:  [](void* p) { delete (T*)p; }
     *             it is used to destroy an element.
     * @param cap  max capacity of the pool for each thread, -1 for unlimited.
     *             this argument is ignored if dcb is NULL.
     *             default: -1.
     */
    Pool(std::function<void* ()>&& ccb, std::function<void(void*)>&& dcb, size_t cap=(size_t)-1);

    Pool(Pool&& p) : _p(p._p) { p._p = 0; }

    /**
     * pop an element from the pool
     *   - It MUST be called in a coroutine.
     *   - If the pool is not empty, the element at the back will be popped, otherwise
     *     ccb is used to create a new element if ccb is not NULL.
     *
     * @return  a pointer to an element, or NULL if the pool is empty and the ccb is NULL.
     */
    void* pop();

    /**
     * push an element to the pool
     *   - It MUST be called in a coroutine.
     *
     * @param e  a pointer to an element, it will be ignored if it is NULL.
     */
    void push(void* e);

    /**
     * return size of the internal pool for the current thread.
     */
    size_t size() const;

  private:
    void* _p;
    DISALLOW_COPY_AND_ASSIGN(Pool);
};

/**
 * guard to push an element back to co::Pool
 *   - Pool::pop() is called in the constructor.
 *   - Pool::push() is called in the destructor.
 *   - operator->() is overloaded, so it has a behavior similar to std::unique_ptr.
 *
 *   - usage:
 *     struct T { void hello(); };
 *     co::Pool pool(
 *         []() { return (void*) new T; },  // ccb
 *         [](void* p) { delete (T*)p; }    // dcb
 *     );
 *
 *     co::PoolGuard<T> g(pool);
 *     g->hello();
 */
template<typename T>
class PoolGuard {
  public:
    explicit PoolGuard(Pool& pool) : _pool(pool) {
        _p = (T*)_pool.pop();
    }

    explicit PoolGuard(Pool* pool) : _pool(*pool) {
        _p = (T*)_pool.pop();
    }

    ~PoolGuard() {
        _pool.push(_p);
    }

    /**
     * overload operator-> for PoolGuard
     *
     * @return  a pointer to an object of class T.
     */
    T* operator->() const { assert(_p); return _p; }

    bool operator==(T* p) const { return _p == p; }
    bool operator!=(T* p) const { return _p != p; }
    bool operator!() const { return _p == NULL; }
    explicit operator bool() const { return _p != NULL; }

    /**
     * get the pointer owns by PoolGuard
     *
     * @return  a pointer to an object of class T
     */
    T* get() const { return _p; }

    /**
     * reset the pointer owns by PoolGuard
     *
     * @param p  a pointer to an object of class T, it MUST be created with operator new.
     *           default: NULL.
     */
    void reset(T* p = 0) {
        if (_p != p) { delete _p; _p = p; }
    }

    /**
     * assign a new pointer to PoolGuard
     *
     * @param p  a pointer to an object of class T, it MUST be created with operator new.
     */
    void operator=(T* p) { this->reset(p); }

  private:
    Pool& _pool;
    T* _p;
    DISALLOW_COPY_AND_ASSIGN(PoolGuard);
};

#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning (disable:4200)
#endif

/**
 * IoEvent is for waiting an IO event on a socket 
 *   - It MUST be used in a coroutine. 
 *   - This is different from IoEvent for linux & mac. Windows users should be 
 *     careful. See details in the comments below.
 */
class IoEvent {
  public:
    struct PerIoInfo {
        WSAOVERLAPPED ol;
        void* co;     // user data, pointer to a coroutine
        DWORD n;      // bytes transfered
        DWORD flags;  // flags for WSARecv, WSARecvFrom
        union {
            void* _;
            int ios;  // io state:  io_done: 1, io_timeout: 2
        };
        WSABUF buf;   // buffer for WSARecv, WSARecvFrom, WSASend, WSASendTo
        char s[];     // extra buffer allocated
    };
    
    static const int nb_tcp_recv = 1; // recv on non-blocking tcp socket
    static const int nb_tcp_send = 2; // send on non-blocking tcp socket
    static const int nb_tcp_conn = 4; // connect on non-blocking tcp socket

    /**
     * the constructor with IO event on a non-blocking TCP socket 
     *   - This case works for non-blocking TCP sockets only.
     *   - eg.
     *     co::IoEvent ev(fd, co::ev_read);
     *     do {
     *         int r = ::recv(fd, buf, n, 0);
     *         if (r != -1) return r;
     *         if (co::error() == WSAEWOULDBLOCK) {
     *             if (!ev.wait(ms)) return -1;
     *         } else {
     *             return -1;
     *         }
     *     } while (true);
     * 
     * @param fd  a TCP socket, it MUST be non-blocking and overlapped.
     * @param ev  the IO event, either ev_read or ev_write.
     */
    IoEvent(sock_t fd, io_event_t ev);

    /**
     * the constructor without any IO event 
     *   - This case works for any overlapped sockets.
     *   - If n > 0, an extra n-byte buffer will be allocated with the PerIoInfo, 
     *     and users can use IoEvent->s to access the extra buffer. 
     *   - eg.
     *     IoEvent ev(fd, n);   // alloc extra n bytes for recv
     *     ev->buf.buf = ev->s; // extra buffer
     *     ev->buf.len = n;     // buffer size
     *     int r = WSARecv(fd, &ev->buf, 1, &ev->n, &ev->flags, &ev->ol, 0);
     *     if (r == 0) {
     *         if (!can_skip_iocp_on_success) ev.wait();
     *     } else if (co::error() == WSA_IO_PENDING) {
     *         if (!ev.wait(ms)) return -1;
     *     } else {
     *         return -1;
     *     }
     *     memcpy(user_buf, ev->s, ev->n); // copy data into user buffer
     *     return (int) ev->n;             // return bytes transfered
     * 
     * @param fd  an overlapped socket, support both TCP and UDP, and it is not 
     *            necessary to be non-blocking.
     * @param n   extra bytes to be allocated with PerIoInfo.
     */
    IoEvent(sock_t fd, int n=0);

    IoEvent(sock_t fd, io_event_t ev, const void* buf, int size, int n = 0);

    ~IoEvent();

    PerIoInfo* operator->() const {
        return _info;
    }

    /**
     * wait for an IO event on a socket 
     *   - If the first constructor was used, no data will be transfered by IOCP. 
     *     wait() blocks until the socket is readable or writable, or timeout, or 
     *     any error occured.
     *   - If the second constructor was used, users MUST firstly create an IoEvent, 
     *     and call WSARecv, WSASend or other APIs to post an IO operation to IOCP, 
     *     and then call wait() to wait for the operation to be done.
     * 
     * @param ms  timeout in milliseconds, default: -1, never timeout. 
     * 
     * @return    true if an IO event is present or the IO operation was done, 
     *            or false on timeout or error.
     */
    bool wait(uint32 ms=(uint32)-1);

  private:
    sock_t _fd;
    PerIoInfo* _info;
    char* _to;
    int _nb_tcp; // for non-blocking tcp socket
    DISALLOW_COPY_AND_ASSIGN(IoEvent);
};

#else
/**
 * IoEvent is for waiting an IO event on a socket 
 *   - It MUST be used in a coroutine. 
 *   - The socket MUST be non-blocking on Linux & Mac. 
 */
class IoEvent {
  public:
    /**
     * the constructor 
     * 
     * @param fd  a non-blocking socket.
     * @param ev  the IO event, either ev_read or ev_write.
     */
    IoEvent(sock_t fd, io_event_t ev)
        : _fd(fd), _ev(ev), _has_ev(false) {
    }

    // remove IO event from epoll or kqueue.
    ~IoEvent();

    /**
     * wait for an IO event on a socket 
     *   - wait() blocks until the IO event is present, or timeout, or any error occured. 
     * 
     * @param ms  timeout in milliseconds, default: -1, never timeout. 
     * 
     * @return    true if an IO event is present, or false on timeout or error.
     */
    bool wait(uint32 ms=(uint32)-1);

  private:
    sock_t _fd;
    io_event_t _ev;
    bool _has_ev;
    DISALLOW_COPY_AND_ASSIGN(IoEvent);
};
#endif

class WaitGroup {
  public:
    WaitGroup();
    ~WaitGroup();
    WaitGroup(WaitGroup&& wg) : _p(wg._p) { wg._p = 0; }

    /**
     * increase WaitGroup counter by n
     * 
     * @param n  1 by default.
     */
    void add(uint32 n=1);

    /**
     * decrease WaitGroup counter by 1
     */
    void done();

    /**
     * blocks until the counter becomes 0
     */
    void wait();

  private:
    void* _p;
    DISALLOW_COPY_AND_ASSIGN(WaitGroup);
};

/** 
 * create a socket suitable for coroutine programing
 * 
 * @param domain  address family, AF_INET, AF_INET6, etc.
 * @param type    socket type, SOCK_STREAM, SOCK_DGRAM, etc.
 * @param proto   protocol, IPPROTO_TCP, IPPROTO_UDP, etc.
 * 
 * @return        a non-blocking (also overlapped on windows) socket on success, 
 *                or -1 on error.
 */
sock_t socket(int domain, int type, int proto);

/**
 * create a TCP socket suitable for coroutine programing
 * 
 * @param domain  address family, AF_INET, AF_INET6, etc.
 *                default: AF_INET.
 * 
 * @return        a non-blocking (also overlapped on windows) socket on success, 
 *                or -1 on error.
 */
inline sock_t tcp_socket(int domain=AF_INET) {
    return co::socket(domain, SOCK_STREAM, IPPROTO_TCP);
}

/**
 * create a UDP socket suitable for coroutine programing
 * 
 * @param domain  address family, AF_INET, AF_INET6, etc.
 *                default: AF_INET.
 * 
 * @return        a non-blocking (also overlapped on windows) socket on success, 
 *                or -1 on error.
 */
inline sock_t udp_socket(int domain=AF_INET) {
    return co::socket(domain, SOCK_DGRAM, IPPROTO_UDP);
}

/**
 * close a socket 
 *   - A socket MUST be closed in the same thread that performed the I/O operation, 
 *     usually, in the coroutine where the user called recv(), send(), etc. 
 *   - EINTR has been handled internally. The user need not consider about it. 
 *     
 * @param fd  a non-blocking (also overlapped on windows) socket.
 * @param ms  if ms > 0, the socket will be closed ms milliseconds later. 
 *            default: 0.
 * 
 * @return    0 on success, -1 on error.
 */
int close(sock_t fd, int ms=0);

/**
 * shutdown a socket 
 *   - Like the close(), shutdown() MUST be called in the same thread that performed 
 *     the I/O operation. 
 * 
 * @param fd  a non-blocking (also overlapped on windows) socket.
 * @param c   'r' for SHUT_RD, 'w' for SHUT_WR, 'b' for SHUT_RDWR. 
 *            default: 'b'.
 * 
 * @return    0 on success, -1 on error.
 */
int shutdown(sock_t fd, char c='b');

/**
 * bind an address to a socket
 * 
 * @param fd       a non-blocking (also overlapped on windows) socket.
 * @param addr     a pointer to struct sockaddr, sockaddr_in or sockaddr_in6.
 * @param addrlen  size of the structure pointed to by addr.
 * 
 * @return         0 on success, -1 on error.
 */
int bind(sock_t fd, const void* addr, int addrlen);

/**
 * listen on a socket
 * 
 * @param fd       a non-blocking (also overlapped on windows) socket.
 * @param backlog  maximum length of the queue for pending connections.
 *                 default: 1024.
 * 
 * @return         0 on success, -1 on error.
 */
int listen(sock_t fd, int backlog=1024);

/**
 * accept a connection on a socket 
 *   - It MUST be called in a coroutine. 
 *   - accept() blocks until a connection was present or any error occured. 
 * 
 * @param fd       a non-blocking (also overlapped on windows) socket.
 * @param addr     a pointer to struct sockaddr, sockaddr_in or sockaddr_in6.
 * @param addrlen  the user MUST initialize it with the size of the structure pointed to 
 *                 by addr; on return it contains the actual size of the peer address.
 * 
 * @return         a non-blocking (also overlapped on windows) socket on success,  
 *                 or -1 on error.
 */
sock_t accept(sock_t fd, void* addr, int* addrlen);

/**
 * connect to an address 
 *   - It MUST be called in a coroutine. 
 *   - connect() blocks until the connection was done or timeout, or any error occured. 
 *   - Users can call co::error() to get the errno, and call co::timeout() to check whether it has timed out. 
 * 
 * @param fd       a non-blocking (also overlapped on windows) socket.
 * @param addr     a pointer to struct sockaddr, sockaddr_in or sockaddr_in6.
 * @param addrlen  size of the structure pointed to by addr.
 * @param ms       timeout in milliseconds, if ms < 0, it will never time out. 
 *                 default: -1.
 * 
 * @return         0 on success, -1 on timeout or error.
 */
int connect(sock_t fd, const void* addr, int addrlen, int ms=-1);

/**
 * recv data from a socket 
 *   - It MUST be called in a coroutine. 
 *   - It blocks until any data recieved or timeout, or any error occured. 
 *   - The errno will be set to ETIMEDOUT on timeout, call co::error() to get the errno, 
 *     or simply call co::timeout() to check whether it has timed out. 
 * 
 * @param fd   a non-blocking (also overlapped on windows) socket.
 * @param buf  a pointer to the buffer to recieve the data.
 * @param n    size of the buffer.
 * @param ms   timeout in milliseconds, if ms < 0, it will never time out.
 *             default: -1.
 * 
 * @return     bytes recieved on success, -1 on timeout or error, 0 will be returned 
 *             if fd is a stream socket and the peer has closed the connection.
 */
int recv(sock_t fd, void* buf, int n, int ms=-1);

/**
 * recv n bytes from a socket 
 *   - It MUST be called in a coroutine. 
 *   - It blocks until all the n bytes are recieved or timeout, or any error occured. 
 *   - The errno will be set to ETIMEDOUT on timeout, call co::error() to get the errno, 
 *     or simply call co::timeout() to check whether it has timed out. 
 * 
 * @param fd   a non-blocking (also overlapped on windows) socket, 
 *             it MUST be a stream socket, usually a TCP socket.
 * @param buf  a pointer to the buffer to recieve the data.
 * @param n    bytes to be recieved.
 * @param ms   timeout in milliseconds, if ms < 0, it will never time out.
 *             default: -1.
 * 
 * @return     n on success, -1 on timeout or error, 0 will be returned if the peer 
 *             close the connection.
 */
int recvn(sock_t fd, void* buf, int n, int ms=-1);

/**
 * recv data from a socket 
 *   - It MUST be called in a coroutine. 
 *   - It blocks until any data recieved or timeout, or any error occured. 
 *   - The errno will be set to ETIMEDOUT on timeout, call co::error() to get the errno, 
 *     or simply call co::timeout() to check whether it has timed out. 
 *   - Set src_addr and addrlen to NULL if the user is not interested in the source address. 
 * 
 * @param fd        a non-blocking (also overlapped on windows) socket.
 * @param buf       a pointer to the buffer to recieve the data.
 * @param n         size of the buffer.
 * @param src_addr  a pointer to struct sockaddr, sockaddr_in or sockaddr_in6, the source address 
 *                  will be placed in the buffer pointed to by src_addr.
 * @param addrlen   it should be initialized to the size of the buffer pointed to by src_addr, on 
 *                  return, it contains the actual size of the source address.
 * @param ms        timeout in milliseconds, if ms < 0, it will never time out.
 *                  default: -1.
 * 
 * @return          bytes recieved on success, -1 on timeout or error, 0 will be returned 
 *                  if fd is a stream socket and the peer has closed the connection.
 */
int recvfrom(sock_t fd, void* buf, int n, void* src_addr, int* addrlen, int ms=-1);

/**
 * send n bytes on a socket 
 *   - It MUST be called in a coroutine. 
 *   - It blocks until all the n bytes are sent or timeout, or any error occured. 
 *   - The errno will be set to ETIMEDOUT on timeout, call co::error() to get the errno, 
 *     or simply call co::timeout() to check whether it has timed out. 
 * 
 * @param fd   a non-blocking (also overlapped on windows) socket.
 * @param buf  a pointer to a buffer of the data to be sent.
 * @param n    size of the data.
 * @param ms   timeout in milliseconds, if ms < 0, it will never time out. 
 *             default: -1.
 * 
 * @return     n on success, or -1 on error. 
 */
int send(sock_t fd, const void* buf, int n, int ms=-1);

/**
 * send n bytes on a socket 
 *   - It MUST be called in a coroutine. 
 *   - It blocks until all the n bytes are sent or timeout, or any error occured. 
 *   - The errno will be set to ETIMEDOUT on timeout, call co::error() to get the errno, 
 *     or simply call co::timeout() to check whether it has timed out. 
 * 
 * @param fd        a non-blocking (also overlapped on windows) socket.
 * @param buf       a pointer to a buffer of the data to be sent.
 * @param n         size of the data, n MUST be no more than 65507 if fd is an udp socket.
 * @param dst_addr  a pointer to struct sockaddr, sockaddr_in or sockaddr_in6, which contains 
 *                  the destination address, SHOULD be NULL if fd is a tcp socket.
 * @param addrlen   size of the destination address.
 * @param ms        timeout in milliseconds, if ms < 0, it will never time out. 
 *                  default: -1.
 * 
 * @return          n on success, -1 on timeout or error. 
 */
int sendto(sock_t fd, const void* buf, int n, const void* dst_addr, int addrlen, int ms=-1);

#ifdef _WIN32
/**
 * get options on a socket, man getsockopt for details.
 */
inline int getsockopt(sock_t fd, int lv, int opt, void* optval, int* optlen) {
    return ::getsockopt(fd, lv, opt, (char*)optval, optlen);
}

/**
 * set options on a socket, man setsockopt for details. 
 */
inline int setsockopt(sock_t fd, int lv, int opt, const void* optval, int optlen) {
    return ::setsockopt(fd, lv, opt, (const char*)optval, optlen);
}

#else
/**
 * get options on a socket, man getsockopt for details.
 */
inline int getsockopt(sock_t fd, int lv, int opt, void* optval, int* optlen) {
    return ::getsockopt(fd, lv, opt, optval, (socklen_t*)optlen);
}

/**
 * set options on a socket, man setsockopt for details. 
 */
inline int setsockopt(sock_t fd, int lv, int opt, const void* optval, int optlen) {
    return ::setsockopt(fd, lv, opt, optval, (socklen_t)optlen);
}
#endif

/**
 * set option SO_REUSEADDR on a socket
 */
inline void set_reuseaddr(sock_t fd) {
    int v = 1;
    co::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
}

/**
 * set send buffer size for a socket 
 *   - It MUST be called before the socket is connected. 
 */
inline void set_send_buffer_size(sock_t fd, int n) {
    co::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n));
}

/**
 * set recv buffer size for a socket 
 *   - It MUST be called before the socket is connected. 
 */
inline void set_recv_buffer_size(sock_t fd, int n) {
    co::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));
}

/**
 * set option TCP_NODELAY on a TCP socket  
 */
inline void set_tcp_nodelay(sock_t fd) {
    int v = 1;
    co::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v));
}

/**
 * set option SO_KEEPALIVE on a TCP socket 
 */
inline void set_tcp_keepalive(sock_t fd) {
    int v = 1;
    co::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &v, sizeof(v));
}

/**
 * reset a TCP connection 
 *   - It MUST be called in the same thread that performed the IO operation. 
 *   - It is usually used in a server to avoid TIME_WAIT status. 
 *     
 * @param fd  a non-blocking (also overlapped on windows) socket.
 * @param ms  if ms > 0, the socket will be closed ms milliseconds later.
 *            default: 0.
 * 
 * @return    0 on success, -1 on error.
 */
inline int reset_tcp_socket(sock_t fd, int ms=0) {
    struct linger v = { 1, 0 };
    co::setsockopt(fd, SOL_SOCKET, SO_LINGER, &v, sizeof(v));
    return co::close(fd, ms);
}

#ifdef _WIN32
/**
 * set option O_NONBLOCK on a socket 
 */
void set_nonblock(sock_t fd);

#else
/**
 * set option O_NONBLOCK on a socket 
 */
void set_nonblock(sock_t fd);

/**
 * set option FD_CLOEXEC on a socket 
 */
inline void set_cloexec(sock_t fd) {
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
}
#endif

/**
 * fill in an ipv4 address with ip & port 
 * 
 * @param addr  a pointer to an ipv4 address.
 * @param ip    string like: "127.0.0.1".
 * @param port  a value from 1 to 65535.
 * 
 * @return      true on success, otherwise false.
 */
inline bool init_ip_addr(struct sockaddr_in* addr, const char* ip, int port) {
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = hton16((uint16)port);
    return inet_pton(AF_INET, ip, &addr->sin_addr) == 1;
}

/**
 * fill in an ipv6 address with ip & port
 * 
 * @param addr  a pointer to an ipv6 address.
 * @param ip    string like: "::1".
 * @param port  a value from 1 to 65535.
 * 
 * @return      true on success, otherwise false.
 */
inline bool init_ip_addr(struct sockaddr_in6* addr, const char* ip, int port) {
    memset(addr, 0, sizeof(*addr));
    addr->sin6_family = AF_INET6;
    addr->sin6_port = hton16((uint16) port);
    return inet_pton(AF_INET6, ip, &addr->sin6_addr) == 1;
}

/**
 * get ip string of an ipv4 address 
 */
inline fastring ip_str(const struct sockaddr_in* addr) {
    char s[INET_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET, (void*)&addr->sin_addr, s, sizeof(s));
    return fastring(s);
}

/**
 * get ip string of an ipv6 address
 */
inline fastring ip_str(const struct sockaddr_in6* addr) {
    char s[INET6_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET6, (void*)&addr->sin6_addr, s, sizeof(s));
    return fastring(s);
}

/**
 * convert an ipv4 address to a string 
 *
 * @return  a string in format "ip:port"
 */
inline fastring to_string(const struct sockaddr_in* addr) {
    char s[INET_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET, (void*)&addr->sin_addr, s, sizeof(s));
    const size_t n = strlen(s);
    fastring r(n + 8);
    r.append(s, n).append(':') << ntoh16(addr->sin_port);
    return r;
}

/**
 * convert an ipv6 address to a string 
 *
 * @return  a string in format: "ip:port"
 */
inline fastring to_string(const struct sockaddr_in6* addr) {
    char s[INET6_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET6, (void*)&addr->sin6_addr, s, sizeof(s));
    const size_t n = strlen(s);
    fastring r(n + 8);
    r.append(s, n).append(':') << ntoh16(addr->sin6_port);
    return r;
}

/**
 * convert an ip address to a string 
 * 
 * @param addr  a pointer to struct sockaddr.
 * @param len   length of the addr, sizeof(sockaddr_in) or sizeof(sockaddr_in6).
 */
inline fastring to_string(const void* addr, int len) {
    if (len == sizeof(sockaddr_in)) return to_string((const sockaddr_in*)addr);
    return to_string((const struct sockaddr_in6*)addr);
}

/**
 * get peer address of a connected socket 
 * 
 * @return  a string in format: "ip:port", or an empty string on error.
 */
inline fastring peer(sock_t fd) {
    union {
        struct sockaddr_in  v4;
        struct sockaddr_in6 v6;
    } addr;
    int addrlen = sizeof(addr);
    const int r = getpeername(fd, (sockaddr*)&addr, (socklen_t*)&addrlen);
    if (r == 0) {
        if (addrlen == sizeof(addr.v4)) return co::to_string(&addr.v4);
        if (addrlen == sizeof(addr.v6)) return co::to_string(&addr.v6);
    }
    return fastring();
}

#ifdef _WIN32
/**
 * get the last error number
 */
inline int error() { return WSAGetLastError(); }

/**
 * set the last error number
 */
inline void set_last_error(int err) { WSASetLastError(err); }
#else
/**
 * get the last error number
 */
inline int error() { return errno; }

/**
 * set the last error number
 */
inline void set_last_error(int err) { errno = err; }
#endif

/**
 * get string of a error number 
 *   - It is thread-safe. 
 */
const char* strerror(int err);

/**
 * get string of the current error number 
 *   - It is thread-safe. 
 */
inline const char* strerror() {
    return co::strerror(co::error());
}

} // namespace co

using co::go;
