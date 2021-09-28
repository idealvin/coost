#pragma once

#include "../def.h"
#include "../fastring.h"
#include <functional>
#include <memory>

namespace tcp {

struct __coapi Connection final {
    Connection(int sock);
    Connection(void* ssl);
    ~Connection() { this->close(); }

    Connection(Connection&& c) : _p(c._p) {
        c._p = 0;
    }

    Connection(const Connection&) = delete;
    void operator=(const Connection&) = delete;

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
};

/**
 * TCP server based on coroutine 
 *   - Support both ipv4 and ipv6. 
 *   - Support ssl (openssl required).
 *   - One coroutine per connection. 
 */
class __coapi Server final {
  public:
    Server();
    ~Server();

    /**
     * set a callback for handling a connection 
     *   - The user MUST delete the Connection pointer when the connection was closed.
     * 
     * @param f  either a pointer to void f(tcp::Connection*), 
     *           or a reference of std::function<void(tcp::Connection*)>.
     */
    void on_connection(std::function<void(Connection)>&& f);

    void on_connection(const std::function<void(Connection)>& f) {
        this->on_connection(std::function<void(Connection)>(f));
    }

    /**
     * set a callback for handling a connection 
     *   - The user MUST delete the Connection pointer when the connection was closed.
     * 
     * @param f  a pointer to a method with a parameter of type tcp::Connection* in class T.
     * @param o  a pointer to an object of class T.
     */
    template<typename T>
    void on_connection(void (T::*f)(Connection), T* o) {
        this->on_connection(std::bind(f, o, std::placeholders::_1));
    }

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
     *   - NOTE: The server will not close previously established connections.
     */
    void exit();

  private:
    void* _p;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

/**
 * TCP client based on coroutine 
 *   - Support both ipv4 and ipv6. 
 *   - Support ssl (openssl required).
 *   - One client corresponds to one connection. 
 * 
 *   - It MUST be used in a coroutine. 
 *   - It is NOT coroutine-safe, NEVER use a same Client in different coroutines 
 *     at the same time. 
 * 
 *   - It is recommended to put tcp::Client in co::Pool, when lots of connections 
 *     may be established. 
 */
class __coapi Client {
  public:
    /**
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
    Client(const Client& c)
        : _ip(c._ip), _port(c._port), _use_ssl(c._use_ssl),
          _fd(-1), _ssl(0), _ssl_ctx(0) {
    }

    virtual ~Client() { this->close(); }

    void operator=(const Client& c) = delete;

    /**
     * recv using co::recv or ssl::recv
     * 
     * @return  >0 on success, -1 on timeout or error, 0 will be returned if the 
     *          peer closed the connection.
     */
    virtual int recv(void* buf, int n, int ms=-1);

    /**
     * recv n bytes using co::recvn or ssl::recvn
     * 
     * @return  n on success, -1 on timeout or error, 0 will be returned if the 
     *          peer closed the connection.
     */
    virtual int recvn(void* buf, int n, int ms=-1);

    /**
     * send n bytes using co::send or ssl::send 
     *   - If use SSL, this method may return 0 on error.
     * 
     * @return  n on success, <=0 on timeout or error.
     */
    virtual int send(const void* buf, int n, int ms=-1);

    /**
     * check whether the connection has been established 
     */
    virtual bool connected() const {
        return (_use_ssl && _ssl != NULL) || (_fd != -1);
    }

    /**
     * connect to the server 
     *   - It MUST be called in the thread that performed the IO operation. 
     *
     * @param ms  timeout in milliseconds, -1 for never timeout.
     * 
     * @return    true on success, false on timeout or error.
     */
    virtual bool connect(int ms);

    /**
     * close the connection 
     *   - It MUST be called in the thread that performed the IO operation. 
     */
    virtual void disconnect();

    /**
     * the same as disconnect 
     */
    void close() { this->disconnect(); }

    /**
     * get error string
     */
    virtual const char* strerror() const;

    /**
     * get the socket fd 
     */
    int socket() const { return _fd; }

  protected:
    fastring _ip;
    uint16 _port;
    bool _use_ssl;
    int _fd;
    void* _ssl;
    void* _ssl_ctx;
};

} // tcp
