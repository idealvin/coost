#pragma once

#include "def.h"
#include <functional>

namespace tcp {

/**
 * TCP connection for tcp::Server 
 *   - An object of tcp::Connection will be created by tcp::Server if a connection 
 *     was accepted. DO NOT create tcp::Connection by yourself.
 *   - If tcp::Server is a SSL server, data will be transfered by SSL.
 */
struct __coapi Connection final {
    // normal TCP connection
    Connection(int sock);

    // TCP connection with SSL support. Data will be transfered by SSL.
    Connection(void* ssl);

    // move constructor
    Connection(Connection&& c) : _p(c._p) { c._p = 0; }

    // close the connection in destructor
    ~Connection() { this->close(); }

    // get the underlying socket fd
    int socket() const;

    /**
     * recv using co::recv or ssl::recv
     * 
     * @return  >0 on success, -1 on timeout or error, 0 will be returned if the 
     *          peer closed the connection.
     */
    int recv(void* buf, int n, int ms=-1);

    /**
     * recv n bytes using co::recvn or ssl::recvn
     * 
     * @return  n on success, -1 on timeout or error, 0 will be returned if the 
     *          peer closed the connection.
     */
    int recvn(void* buf, int n, int ms=-1);

    /**
     * send n bytes using co::send or ssl::send 
     *   - If use SSL, this method may return 0 on error.
     * 
     * @return  n on success, <=0 on timeout or error.
     */
    int send(const void* buf, int n, int ms=-1);

    /**
     * close the connection
     *   - Once a Connection was closed, it can't be used any more.
     *
     * @param ms  if ms > 0, the connection will be closed ms milliseconds later.
     */
    int close(int ms = 0);

    /**
     * reset the connection
     *   - Once a Connection was reset, it can't be used any more.
     *   - Server may use this method instead of close() to avoid TIME_WAIT state.
     *
     * @param ms  if ms > 0, the connection will be closed ms milliseconds later.
     */
    int reset(int ms = 0);

    /**
     * get error message of the last I/O operation
     *   - If an error occured in send() or recv(), this method can be called to 
     *     get the error message.
     */
    const char* strerror() const;

  private:
    void* _p;

    DISALLOW_COPY_AND_ASSIGN(Connection);
};

/**
 * TCP server based on coroutine 
 *   - Support both ipv4 and ipv6. 
 *   - Support ssl (openssl 1.1.0+ required).
 *   - One coroutine per connection. 
 */
class __coapi Server final {
  public:
    Server();
    ~Server();

    // set a connection callback
    Server& on_connection(std::function<void(Connection)>&& f);

    Server& on_connection(const std::function<void(Connection)>& f) {
        return this->on_connection(std::function<void(Connection)>(f));
    }

    /**
     * @param f  a pointer to a method in class T.
     * @param o  a pointer to an object of class T.
     */
    template<typename T>
    Server& on_connection(void (T::*f)(Connection), T* o) {
        return this->on_connection(std::bind(f, o, std::placeholders::_1));
    }

    // set a callback to call when the server exits
    Server& on_exit(std::function<void()>&& cb);

    // return number of connections
    uint32 conn_num() const;

    /**
     * start the server
     *   - The server will loop in a coroutine, and it will not block the calling thread.
     *   - The user MUST call on_connection() to set a connection callback before start()
     *     was called.
     *   - By default, key and ca are NULL, and ssl is disabled.
     *
     * @param ip    server ip, either an ipv4 or ipv6 address.
     *              if ip is NULL or empty, "0.0.0.0" will be used by default.
     * @param port  server port.
     * @param key   path of ssl private key file.
     * @param ca    path of ssl certificate file.
     */
    void start(const char* ip, int port, const char* key=0, const char* ca=0);

    /**
     * exit the server gracefully
     *   - Once `exit()` was called, the listening socket will be closed, and new 
     *     connections will not be accepted.
     *   - NOTE: The server will not close previously established connections. To 
     *     close the connections, see the example in test/tcp2.cc.
     */
    void exit();

  private:
    void* _p;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

/**
 * TCP client based on coroutine 
 *   - Support both ipv4 and ipv6. 
 *   - Support ssl (openssl 1.1.0+ required).
 *   - One client corresponds to one connection. 
 * 
 *   - It MUST be used in a coroutine. 
 *   - It is NOT coroutine-safe, DO NOT use a same Client in different coroutines 
 *     at the same time. 
 * 
 *   - It is recommended to put tcp::Client in co::Pool, when lots of connections 
 *     may be established. 
 */
class __coapi Client final {
  public:
    /**
     * the constructor
     *   - NOTE: It will not connect to the server immediately here.
     * 
     * @param ip       a domain name, or either an ipv4 or ipv6 address of the server. 
     *                 if ip is NULL or empty, "127.0.0.1" will be used by default. 
     * @param port     the server port. 
     * @param use_ssl  use ssl if it is true.
     */
    Client(const char* ip, int port, bool use_ssl=false);

    /**
     * copy constructor 
     *   - Copy ip, port, use_ssl from another Client. 
     */
    Client(const Client& c);

    /**
     * the destructor
     *   - Connection will be closed here.
     */
    ~Client();

    void operator=(const Client& c) = delete;

    /**
     * recv using co::recv or ssl::recv
     * 
     * @return  >0 on success, -1 on timeout or error, 0 will be returned if the 
     *          peer closed the connection.
     */
    int recv(void* buf, int n, int ms=-1);

    /**
     * recv n bytes using co::recvn or ssl::recvn
     * 
     * @return  n on success, -1 on timeout or error, 0 will be returned if the 
     *          peer closed the connection.
     */
    int recvn(void* buf, int n, int ms=-1);

    /**
     * send n bytes using co::send or ssl::send 
     *   - If use SSL, this method may return 0 on error.
     * 
     * @return  n on success, <=0 on timeout or error.
     */
    int send(const void* buf, int n, int ms=-1);

    /**
     * @brief bind ip and port to the client socket
     * 
     * @return true on success, false otherwise
     */
    bool bind(const char* ip, int port=0);

    /**
     * check whether the connection has been established 
     */
    bool connected() const noexcept { return _connected; }

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
     *   - It can be called anywhere since v2.0.1. 
     */
    void disconnect();

    // close the connection, the same as disconnect 
    void close() { this->disconnect(); }

    // get error string
    const char* strerror() const;

    // get the socket fd 
    int socket() const noexcept { return _fd; }

  private:
    union {
        uint32* _u;
        char* _p;  // _p+8: port, _p+16: ip
        void** _s; // _s[-1]: ssl, _s[-2]: ssl_ctx
    };
    int _fd;
    bool _use_ssl;
    bool _connected;
};

} // tcp
