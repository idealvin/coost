#include "co/co.h"
#include "co/so/tcp.h"
#include "co/so/ssl.h"
#include "co/log.h"
#include "co/str.h"

DEF_int32(ssl_handshake_timeout, 3000, "#2 ssl handshake timeout in ms");

namespace tcp {

int Connection::recv(void* buf, int n, int ms) {
    return co::recv(_fd, buf, n, ms);
}

int Connection::recvn(void* buf, int n, int ms) {
    return co::recvn(_fd, buf, n, ms);
}

int Connection::send(const void* buf, int n, int ms) {
    return co::send(_fd, buf, n, ms);
}

int Connection::close(int ms) {
    if (_fd != -1) {
        int r = co::close(_fd, ms);
        _fd = -1;
        return r;
    }
    return 0;
}

int Connection::reset(int ms) {
    if (_fd != -1) {
        int r = co::reset_tcp_socket(_fd, ms);
        _fd = -1;
        return r;
    }
    return 0;
}

const char* Connection::strerror() const {
    return co::strerror();
}

#ifdef CO_SSL
struct SSLConnection : public tcp::Connection {
    SSLConnection(SSL* ssl) : tcp::Connection(ssl::get_fd(ssl)), s(ssl) {}
    virtual ~SSLConnection() { this->close(); };

    virtual int recv(void* buf, int n, int ms=-1) {
        return ssl::recv(s, buf, n, ms);
    }

    virtual int recvn(void* buf, int n, int ms=-1) {
        return ssl::recvn(s, buf, n, ms);
    }

    virtual int send(const void* buf, int n, int ms=-1) {
        return ssl::send(s, buf, n, ms);
    }

    virtual int close(int ms=0) {
        if (s) {
            ssl::shutdown(s);
            ssl::free_ssl(s);
            s = 0;
            return tcp::Connection::close(ms);
        }
        return 0;
    }

    virtual int reset(int ms=0) {
        if (s) {
            ssl::free_ssl(s);
            s = 0;
            return tcp::Connection::reset(ms);
        }
        return 0;
    }

    virtual const char* strerror() const {
        return ssl::strerror(s);
    }

    SSL* s;
};
#endif

struct ServerParam {
    ServerParam(const char* ip, int port, std::function<void(Connection*)>&& on_connection)
        : ip((ip && *ip) ? ip : "0.0.0.0"), port(str::from(port)), 
          on_connection(std::move(on_connection)), ssl_ctx(0) {
    }

    ~ServerParam() {
      #ifdef CO_SSL
        if (ssl_ctx) ssl::free_ctx((SSL_CTX*)ssl_ctx);
      #endif
    }

    fastring ip;
    fastring port;
    sock_t fd;
    sock_t connfd;
    std::function<void(Connection*)> on_connection;
    std::function<void(sock_t)> on_conn_fd;
    void* ssl_ctx; // SSL_CTX

    // use union here to support both ipv4 and ipv6
    union {
        struct sockaddr_in  v4;
        struct sockaddr_in6 v6;
    } addr;
    int addrlen;
};

/**
 * the server loop 
 *   - It listens on a port and waits for connections. 
 *   - When a connection is accepted, it will start a new coroutine and call 
 *     the connection callback to handle the connection. 
 */
static void server_loop(void* p);

static void on_tcp_connection(ServerParam* p, sock_t fd) {
    co::set_tcp_keepalive(fd);
    co::set_tcp_nodelay(fd);
    p->on_connection(new tcp::Connection((int)fd));
}

#ifdef CO_SSL
static void on_ssl_connection(ServerParam* p, sock_t fd) {
    co::set_tcp_keepalive(fd);
    co::set_tcp_nodelay(fd);

    SSL* s = ssl::new_ssl((SSL_CTX*)p->ssl_ctx);
    if (s == NULL) goto new_ssl_err;
    if (ssl::set_fd(s, (int)fd) != 1) goto set_fd_err;
    if (ssl::accept(s, FLG_ssl_handshake_timeout) <= 0) goto accept_err;

    p->on_connection(new SSLConnection(s));
    return;

  new_ssl_err:
    ELOG << "new SSL failed: " << ssl::strerror();
    goto err_end;
  set_fd_err:
    ELOG << "ssl set fd (" << fd << ") failed: " << ssl::strerror(s);
    goto err_end;
  accept_err:
    ELOG << "ssl accept failed: " << ssl::strerror(s);
    goto err_end;
  err_end:
    if (s) ssl::free_ssl(s);
    co::close(fd, 1000);
    return;
}
#endif

void Server::start(const char* ip, int port, const char* key, const char* ca) {
    CHECK(_on_connection != NULL) << "connection callback not set..";
    ServerParam* p = new ServerParam(ip, port, std::move(_on_connection));

    if (key && *key && ca && *ca) {
      #ifdef CO_SSL
        auto ctx = ssl::new_server_ctx();
        p->ssl_ctx = ctx;
        CHECK(ctx != NULL) << "ssl new server contex error: " << ssl::strerror();

        int r;
        r = ssl::use_private_key_file(ctx, key);
        CHECK_EQ(r, 1) << "ssl use private key file (" << key << ") error: " << ssl::strerror();

        r = ssl::use_certificate_file(ctx, ca);
        CHECK_EQ(r, 1) << "ssl use certificate file (" << ca << ") error: " << ssl::strerror();

        r = ssl::check_private_key(ctx);
        CHECK_EQ(r, 1) << "ssl check private key error: " << ssl::strerror();

        p->on_conn_fd = std::bind(on_ssl_connection, p, std::placeholders::_1);
        go(server_loop, (void*)p);
      #else
        CHECK(false) << "openssl must be installed..";
      #endif
    } else {
        p->on_conn_fd = std::bind(on_tcp_connection, p, std::placeholders::_1);
        go(server_loop, (void*)p);
    }
}

void server_loop(void* arg) {
    std::unique_ptr<ServerParam> p((ServerParam*)arg);

    do {
        struct addrinfo* info = 0;
        int r = getaddrinfo(p->ip.c_str(), p->port.c_str(), NULL, &info);
        CHECK_EQ(r, 0) << "invalid ip address: " << p->ip << ':' << p->port;
        CHECK(info != NULL);

        p->fd = co::tcp_socket(info->ai_family);
        CHECK_NE(p->fd, (sock_t)-1) << "create socket error: " << co::strerror();
        co::set_reuseaddr(p->fd);

        // turn off IPV6_V6ONLY
        if (info->ai_family == AF_INET6) {
            int on = 0;
            co::setsockopt(p->fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
        }

        r = co::bind(p->fd, info->ai_addr, (int)info->ai_addrlen);
        CHECK_EQ(r, 0) << "bind (" << p->ip << ':' << p->port << ") failed: " << co::strerror();

        r = co::listen(p->fd, 1024);
        CHECK_EQ(r, 0) << "listen error: " << co::strerror();

        freeaddrinfo(info);
    } while (0);

    LOG << "server " << p->fd << " start: " << p->ip << ':' << p->port;
    while (true) {
        p->addrlen = sizeof(p->addr);
        p->connfd = co::accept(p->fd, &p->addr, &p->addrlen);
        if (unlikely(p->connfd == (sock_t)-1)) {
            WLOG << "server " << p->fd << " accept error: " << co::strerror();
            continue;
        }

        DLOG << "server " << p->fd << " accept new connection: "
             << co::to_string(&p->addr, p->addrlen) << ", connfd: " << p->connfd;
        go(&p->on_conn_fd, p->connfd);
    }
}

Client::Client(const char* ip, int port, bool use_ssl)
    : _ip((ip && *ip) ? ip : "127.0.0.1"), _port((uint16)port),
      _use_ssl(use_ssl), _fd(-1), _ssl(0), _ssl_ctx(0) {
  #ifndef CO_SSL
    CHECK(!use_ssl) << "openssl must be installed..";
  #endif
}

int Client::recv(void* buf, int n, int ms) {
    if (!_use_ssl) return co::recv(_fd, buf, n, ms);
  #ifdef CO_SSL
    return ssl::recv((SSL*)_ssl, buf, n, ms);
  #endif
}

int Client::recvn(void* buf, int n, int ms) {
    if (!_use_ssl) return co::recvn(_fd, buf, n, ms);
  #ifdef CO_SSL
    return ssl::recvn((SSL*)_ssl, buf, n, ms);
  #endif
}

int Client::send(const void* buf, int n, int ms) {
    if (!_use_ssl) return co::send(_fd, buf, n, ms);
  #ifdef CO_SSL
    return ssl::send((SSL*)_ssl, buf, n, ms);
  #endif
}

bool Client::connect(int ms) {
    if (this->connected()) return true;

    fastring port = str::from(_port);
    struct addrinfo* info = 0;
    int r = getaddrinfo(_ip.c_str(), port.c_str(), NULL, &info);
    if (r != 0) goto err;

    CHECK_NOTNULL(info);
    _fd = (int) co::tcp_socket(info->ai_family);
    if (_fd == -1) {
        ELOG << "connect to " << _ip << ':' << _port << " failed, create socket error: " << co::strerror();
        goto err;
    }

    r = co::connect(_fd, info->ai_addr, (int)info->ai_addrlen, ms);
    if (r == -1) {
        ELOG << "connect to " << _ip << ':' << _port << " failed: " << co::strerror();
        goto err;
    }

    co::set_tcp_nodelay(_fd);

  #ifdef CO_SSL
    if (_use_ssl) {
        if ((_ssl_ctx = ssl::new_client_ctx()) == NULL) goto new_ctx_err;
        if ((_ssl = ssl::new_ssl((SSL_CTX*)_ssl_ctx)) == NULL) goto new_ssl_err;
        if (ssl::set_fd((SSL*)_ssl, _fd) != 1) goto set_fd_err;
        if (ssl::connect((SSL*)_ssl, ms) != 1) goto connect_err;
    }
  #endif

    if (info) freeaddrinfo(info);
    return true;

  #ifdef CO_SSL
  new_ctx_err:
    ELOG << "ssl connect new client contex error: " << ssl::strerror();
    goto err_end;
  new_ssl_err:
    ELOG << "ssl connect new SSL failed: " << ssl::strerror();
    goto err_end;
  set_fd_err:
    ELOG << "ssl connect set fd (" << _fd << ") failed: " << ssl::strerror((SSL*)_ssl);
    goto err_end;
  connect_err:
    ELOG << "ssl connect failed: " << ssl::strerror((SSL*)_ssl);
    goto err_end;
  #endif
  err:
  err_end:
    this->disconnect();
    if (info) freeaddrinfo(info);
    return false;
}

void Client::disconnect() {
    if (this->connected()) {
      #ifdef CO_SSL
        if (_ssl) { ssl::free_ssl((SSL*)_ssl); _ssl = 0; }
        if (_ssl_ctx) { ssl::free_ctx((SSL_CTX*)_ssl_ctx); _ssl_ctx = 0; }
      #endif
        co::close(_fd); _fd = -1;
    }
}

const char* Client::strerror() const {
    if (!_use_ssl) return co::strerror();
  #ifdef CO_SSL
    return ssl::strerror((SSL*)_ssl);
  #endif
}

} // tcp
