#pragma once

#include "../co.h"

namespace tcp {

/**
 * TCP server based on coroutine 
 *   - Support both ipv4 and ipv6. 
 *   - One coroutine per connection. 
 */
class Server {
  public:
    Server() = default;
    ~Server() = default;

    /**
     * start the server
     *   - The server will loop in a coroutine, and it will not block the calling thread. 
     *   - The user MUST call on_connection() to set a connection callback before start() 
     *     was called. 
     * 
     * @param ip    either an ipv4 or ipv6 address. 
     *              if ip is NULL or empty, "0.0.0.0" will be used by default. 
     * @param port  the listening port. 
     */
    void start(const char* ip, int port);

    /**
     * set a callback for handling a connection 
     * 
     * @param f  either a pointer to void f(sock_t), 
     *           or a reference of std::function<void(sock_t)>.
     */
    void on_connection(std::function<void(sock_t)>&& f) {
        _on_connection = std::move(f);
    }

    /**
     * set a callback for handling a connection 
     * 
     * @param f  a pointer to a method with a parameter of type sock_t in class T.
     * @param o  a pointer to an object of class T.
     */
    template<typename T>
    void on_connection(void (T::*f)(sock_t), T* o) {
        _on_connection = std::bind(f, o, std::placeholders::_1);
    }

  private:
    std::function<void(sock_t)> _on_connection;

    /**
     * the server loop 
     *   - It listens on a port and waits for connections. 
     *   - When a connection is accepted, it will start a new coroutine and call 
     *     the connection callback to handle the connection. 
     */
    void loop(void* p);

    DISALLOW_COPY_AND_ASSIGN(Server);
};

/**
 * TCP client based on coroutine 
 *   - Support both ipv4 and ipv6. 
 *   - One client corresponds to one connection. 
 * 
 *   - It MUST be used in a coroutine. 
 *   - It is NOT coroutine-safe, NEVER use a same Client in different coroutines 
 *     at the same time. 
 * 
 *   - It is recommended to put tcp::Client in co::Pool, when lots of connections 
 *     may be established. 
 */
class Client {
  public:
    /**
     * @param ip    a domain name, or either an ipv4 or ipv6 address of the server. 
     *              if ip is NULL or empty, "127.0.0.1" will be used by default. 
     * @param port  the server port. 
     */
    Client(const char* ip, int port)
        : _ip((ip && *ip) ? ip : "127.0.0.1"), _port(port),
          _fd((sock_t)-1), _sched_id(-1) {
    }

    ~Client() { this->disconnect(); }

    int recv(void* buf, int n, int ms=-1) {
        return co::recv(_fd, buf, n, ms);
    }

    int recvn(void* buf, int n, int ms=-1) {
        return co::recvn(_fd, buf, n, ms);
    }

    int send(const void* buf, int n, int ms=-1) {
        return co::send(_fd, buf, n, ms);
    }

    /**
     * check whether the connection has been established 
     */
    bool connected() const {
        return _fd != (sock_t)-1;
    }

    /**
     * connect to the server 
     *   - It MUST be called in the thread that performed the IO operation. 
     *
     * @param ms  timeout in milliseconds, -1 for never timeout.
     * 
     * @return    true on success, false on timeout or error.
     */
    bool connect(int ms);

    /**
     * close the connection 
     *   - It MUST be called in the thread that performed the IO operation. 
     */
    void disconnect();

    /**
     * get the socket fd 
     */
    sock_t fd() const { return _fd; }

  protected:
    fastring _ip;
    uint32 _port;
    int _sched_id;  // id of scheduler where this client runs in
    sock_t _fd;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

struct Connection {
    Connection(sock_t s) : fd(s) {}
    virtual ~Connection() = default;

    virtual int recv(void* buf, int n, int ms) {
        return co::recv(fd, buf, n, ms);
    }

    virtual int recvn(void* buf, int n, int ms) {
        return co::recvn(fd, buf, n, ms);
    }

    virtual int send(const void* buf, int n, int ms) {
        return co::send(fd, buf, n, ms);
    }

    /**
     * close the connection 
     *
     * @param ms  if ms > 0, the connection will be closed ms milliseconds later.
     */
    virtual int close(int ms=0) {
        return co::close(fd, ms);
    }

    /**
     * reset the connection 
     * 
     * @param ms  if ms > 0, the connection will be closed ms milliseconds later.
     */
    virtual int reset(int ms=0) {
        return co::reset_tcp_socket(fd, ms);
    }

    virtual const char* strerror() const {
        return co::strerror();
    }

    sock_t fd;
};

} // tcp
