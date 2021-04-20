#pragma once

#include "../co.h"

namespace so {
namespace tcp {

/**
 * TCP server based on coroutine 
 *   - Support both ipv4 and ipv6. 
 *   - One coroutine per connection. 
 */
class Server {
  public:
    Server() = default;
    virtual ~Server() = default;

    /**
     * start the server
     *   - The server will loop in a coroutine, and it will not block the calling thread. 
     * 
     * @param ip    either an ipv4 or ipv6 address. 
     *              if ip is NULL or empty, "0.0.0.0" will be used by default. 
     * @param port  the listening port. 
     */
    virtual void start(const char* ip, int port);

  private:
    /**
     * method for handling a connection 
     *   - The derived class MUST implement this method. 
     *   - This method will run in a coroutine. 
     * 
     * @param fd  a connection socket.
     */
    virtual void on_connection(sock_t fd) = 0;

    /**
     * the server loop 
     *   - It listens on a port and waits for connections.
     *   - When a connection is accepted, it will call go(on_connection...) to 
     *     start a new coroutine to handle the connection. 
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

    virtual ~Client() { this->disconnect(); }

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

} // tcp
} // so

namespace tcp = so::tcp;
