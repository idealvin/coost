#include "co/co.h"
#include "co/god.h"
#include "co/mem.h"
#include "co/tcp.h"
#include "co/ssl.h"
#include "co/log.h"
#include "co/str.h"
#include "co/time.h"

DEF_int32(ssl_handshake_timeout, 3000, ">>#2 ssl handshake timeout in ms");

namespace tcp {

class Conn {
  public:
    Conn() = default;
    virtual ~Conn() = default;

    virtual int recv(void* buf, int n, int ms) = 0;
    virtual int recvn(void* buf, int n, int ms) = 0;
    virtual int send(const void* buf, int n, int ms) = 0;

    virtual int close(int ms) = 0;
    virtual int reset(int ms) = 0;

    virtual int socket() = 0;
    virtual const char* strerror() = 0;
};

class TcpConn : public Conn {
  public:
    TcpConn(int sock) : _sock(sock) {}
    virtual ~TcpConn() { this->close(0); }

    virtual int recv(void* buf, int n, int ms) {
        return co::recv(_sock, buf, n, ms);
    }

    virtual int recvn(void* buf, int n, int ms) {
        return co::recvn(_sock, buf, n, ms);
    }

    virtual int send(const void* buf, int n, int ms) {
        return co::send(_sock, buf, n, ms);
    }

    virtual int close(int ms) {
        const int sock = god::swap(&_sock, -1);
        return sock != -1 ? co::close(sock, ms) : 0;
    }

    virtual int reset(int ms) {
        const int sock = god::swap(&_sock, -1);
        return sock != -1 ? co::reset_tcp_socket(sock, ms) : 0;
    }

    virtual int socket() {
        return _sock;
    }

    virtual const char* strerror() {
        return co::strerror();
    }

  private:
    int _sock;
};

class SSLConn : public Conn {
  public:
    SSLConn(ssl::S* s) : _s(s) {}
    virtual ~SSLConn() { this->close(0); }

    virtual int recv(void* buf, int n, int ms) {
        return ssl::recv(_s, buf, n, ms);
    }

    virtual int recvn(void* buf, int n, int ms) {
        return ssl::recvn(_s, buf, n, ms);
    }

    virtual int send(const void* buf, int n, int ms) {
        return ssl::send(_s, buf, n, ms);
    }

    virtual int close(int ms) {
        ssl::S* s = god::swap(&_s, nullptr);
        if (s) {
            int fd = ssl::get_fd(s);
            ssl::shutdown(s);
            ssl::free_ssl(s);
            if (fd != -1) return co::close(fd, ms);
        }
        return 0;
    }

    virtual int reset(int ms) {
        ssl::S* s = god::swap(&_s, nullptr);
        if (s) {
            int fd = ssl::get_fd(s);
            ssl::free_ssl(s);
            if (fd != -1) return co::reset_tcp_socket(fd, ms);
        }
        return 0;
    }

    virtual int socket() {
        return _s ? ssl::get_fd(_s) : -1;
    }

    virtual const char* strerror() {
        return ssl::strerror(_s);
    }

  private:
    ssl::S* _s;
};

Connection::Connection(int sock) {
    _p = co::make<TcpConn>(sock);
}

Connection::Connection(void* s) {
    _p = co::make<SSLConn>((ssl::S*)s);
}

int Connection::socket() const {
    return ((Conn*)_p)->socket();
}

int Connection::recv(void* buf, int n, int ms) {
    return ((Conn*)_p)->recv(buf, n, ms);
}

int Connection::recvn(void* buf, int n, int ms) {
    return ((Conn*)_p)->recvn(buf, n, ms);
}

int Connection::send(const void* buf, int n, int ms) {
    return ((Conn*)_p)->send(buf, n, ms);
}

int Connection::close(int ms) {
    Conn* p = (Conn*) god::swap(&_p, nullptr);
    if (p) {
        int r = p->close(ms);
        co::del(p); // this is ok, sizeof TcpConn, SSLConn: < 4k
        return r;
    }
    return 0;
}

int Connection::reset(int ms) {
    Conn* p = (Conn*) god::swap(&_p, nullptr);
    if (p) {
        int r = p->reset(ms);
        co::del(p);
        return r;
    }
    return 0;
}

const char* Connection::strerror() const {
    return ((Conn*)_p)->strerror();
}

class ServerImpl {
  public:
    ServerImpl()
        : _started(false), _count(0), _fd((sock_t)-1), _connfd((sock_t)-1),
          _ssl_ctx(0), _status(0) {
    }

    ~ServerImpl() {
        if (_fd != (sock_t)-1) this->exit();
        if (_ssl_ctx) { ssl::free_ctx(_ssl_ctx); _ssl_ctx = 0; }
    }

    void on_connection(std::function<void(Connection)>&& cb) {
        _conn_cb = std::move(cb);
    }

    void on_exit(std::function<void()>&& cb) {
        _exit_cb = std::move(cb);
    }

    void start(const char* ip, int port, const char* key, const char* ca);
    void exit();
    bool started() const { return _started; }

    uint32 conn_num() const { return atomic_load(&_count, mo_relaxed) - 1; }
    uint32 ref() { return atomic_inc(&_count, mo_relaxed); }

    void unref() {
        if (atomic_dec(&_count, mo_acq_rel) == 0) {
            if (_exit_cb) _exit_cb();
            co::del(this);
        }
    }

  private:
    void loop();
    void stop();
    void on_tcp_connection(sock_t sock);
    void on_ssl_connection(sock_t sock);

  private:
    fastring _ip;
    uint16 _port;
    bool _started;
    uint32 _count; // refcount
    sock_t _fd;
    sock_t _connfd;
    std::function<void(Connection)> _conn_cb;
    std::function<void()> _exit_cb;
    std::function<void(sock_t)> _on_sock;
    void* _ssl_ctx;
    int _status;
    int _addrlen;
    union {
        struct sockaddr_in  v4;
        struct sockaddr_in6 v6;
    } _addr;
};

void ServerImpl::start(const char* ip, int port, const char* key, const char* ca) {
    CHECK(_conn_cb != NULL) << "connection callback not set..";
    _ip = (ip && *ip) ? ip : "0.0.0.0";
    _port = (uint16)port;

    if (key && *key && ca && *ca) {
        _ssl_ctx = ssl::new_server_ctx();
        CHECK(_ssl_ctx != NULL) << "ssl new server contex error: " << ssl::strerror();

        int r;
        r = ssl::use_private_key_file(_ssl_ctx, key);
        CHECK_EQ(r, 1) << "ssl use private key file (" << key << ") error: " << ssl::strerror();

        r = ssl::use_certificate_file(_ssl_ctx, ca);
        CHECK_EQ(r, 1) << "ssl use certificate file (" << ca << ") error: " << ssl::strerror();

        r = ssl::check_private_key(_ssl_ctx);
        CHECK_EQ(r, 1) << "ssl check private key error: " << ssl::strerror();

        _on_sock = std::bind(&ServerImpl::on_ssl_connection, this, std::placeholders::_1);
        this->ref();
        atomic_store(&_started, true, mo_relaxed);
        go(&ServerImpl::loop, this);
    } else {
        _on_sock = std::bind(&ServerImpl::on_tcp_connection, this, std::placeholders::_1);
        this->ref();
        atomic_store(&_started, true, mo_relaxed);
        go(&ServerImpl::loop, this);
    }
}

void ServerImpl::exit() {
    int status = atomic_cas(&_status, 0, 1);
    if (status == 2) return; // already stopped

    if (status == 0) {
        sleep::ms(1);
        if (status != 2) go(&ServerImpl::stop, this);
    }

    while (_status != 2) sleep::ms(1);
}

void ServerImpl::stop() {
    const char* ip = (_ip == "0.0.0.0" || _ip == "::") ? "127.0.0.1" : _ip.c_str();
    tcp::Client c(ip, _port);
    c.connect(-1);
}

/**
 * the server loop 
 *   - It listens on a port and waits for connections. 
 *   - When a connection is accepted, it will start a new coroutine and call 
 *     the connection callback to handle the connection. 
 */
void ServerImpl::loop() {
    do {
        fastring port = str::from(_port);
        struct addrinfo* info = 0;
        int r = getaddrinfo(_ip.c_str(), port.c_str(), NULL, &info);
        CHECK_EQ(r, 0) << "invalid ip address: " << _ip << ':' << _port;
        CHECK(info != NULL);

        _fd = co::tcp_socket(info->ai_family);
        CHECK_NE(_fd, (sock_t)-1) << "create socket error: " << co::strerror();
        co::set_reuseaddr(_fd);

        // turn off IPV6_V6ONLY
        if (info->ai_family == AF_INET6) {
            int on = 0;
            co::setsockopt(_fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
        }

        r = co::bind(_fd, info->ai_addr, (int)info->ai_addrlen);
        CHECK_EQ(r, 0) << "bind " << _ip << ':' << _port << " failed: " << co::strerror();

        r = co::listen(_fd, 64 * 1024);
        CHECK_EQ(r, 0) << "listen error: " << co::strerror();

        freeaddrinfo(info);
    } while (0);

    LOG << "server start: " << _ip << ':' << _port;
    while (true) {
        _addrlen = sizeof(_addr);
        _connfd = co::accept(_fd, &_addr, &_addrlen);

        if (unlikely(_status == 1)) {
            co::reset_tcp_socket(_connfd);
            break;
        }

        if (unlikely(_connfd == (sock_t)-1)) {
            WLOG << "server " << _ip << ':' << _port << " accept error: " << co::strerror();
            continue;
        }

        const uint32 n = this->ref() - 1;
        DLOG << "server " << _ip << ':' << _port
             << " accept connection: " << co::to_string(&_addr, _addrlen)
             << ", connfd: " << _connfd << ", conn num: " << n;
        go(&_on_sock, _connfd);
    }

    LOG << "server stopped: " << _ip << ':' << _port;
    co::close(_fd); _fd = (sock_t)-1;
    atomic_store(&_status, 2);
    this->unref();
}

void ServerImpl::on_tcp_connection(sock_t fd) {
    co::set_tcp_keepalive(fd);
    co::set_tcp_nodelay(fd);
    _conn_cb(tcp::Connection((int)fd));
    this->unref();
}

void ServerImpl::on_ssl_connection(sock_t fd) {
    co::set_tcp_keepalive(fd);
    co::set_tcp_nodelay(fd);

    ssl::S* s = ssl::new_ssl((ssl::C*)_ssl_ctx);
    if (s == NULL) goto new_ssl_err;
    if (ssl::set_fd(s, (int)fd) != 1) goto set_fd_err;
    if (ssl::accept(s, FLG_ssl_handshake_timeout) <= 0) goto accept_err;

    _conn_cb(tcp::Connection((void*)s));
    this->unref();
    return;

  new_ssl_err:
    ELOG << "new SSL failed: " << ssl::strerror();
    goto end;
  set_fd_err:
    ELOG << "ssl set fd " << fd << " failed: " << ssl::strerror(s);
    goto end;
  accept_err:
    ELOG << "ssl accept failed: " << ssl::strerror(s);
    goto end;
  end:
    if (s) ssl::free_ssl(s);
    co::close(fd, 1000);
    this->unref();
}

Server::Server() {
    _p = co::make<ServerImpl>();
}

Server::~Server() {
    if (_p) {
        auto p = (ServerImpl*)_p;
        if (!p->started()) co::del(p);
        _p = 0;
    }
}

Server& Server::on_connection(std::function<void(Connection)>&& f) {
    ((ServerImpl*)_p)->on_connection(std::move(f));
    return *this;
}

Server& Server::on_exit(std::function<void()>&& cb) {
    ((ServerImpl*)_p)->on_exit(std::move(cb));
    return *this;
}

uint32 Server::conn_num() const {
    return ((ServerImpl*)_p)->conn_num();
}

void Server::start(const char* ip, int port, const char* key, const char* ca) {
    ((ServerImpl*)_p)->start(ip, port, key, ca);
}

void Server::exit() {
    ((ServerImpl*)_p)->exit();
}

Client::Client(const char* ip, int port, bool use_ssl)
    : _port((uint16)port), _use_ssl(use_ssl), _fd(-1) {
    if (!ip || !*ip) ip = "127.0.0.1";
    const size_t n = strlen(ip) + 1;
    if (!use_ssl) {
        _ip = (char*) co::alloc(n);
        memcpy(_ip, ip, n);
    } else {
        const int h = sizeof(void*) * 2;
        _ip = ((char*)co::alloc(h + n)) + h;
        memcpy(_ip, ip, n);
        _s[-1] = _s[-2] = 0;
    }
}

Client::~Client() {
    this->close();
    if (_ip) {
        const size_t n = strlen(_ip) + 1;
        const int h = sizeof(void*) * 2;
        !_use_ssl ? co::free(_ip, n) : co::free(_ip - h, n + h);
        _ip = 0;
    }
}

int Client::recv(void* buf, int n, int ms) {
    if (!_use_ssl) return co::recv(_fd, buf, n, ms);
    return ssl::recv(_s[-1], buf, n, ms);
}

int Client::recvn(void* buf, int n, int ms) {
    if (!_use_ssl) return co::recvn(_fd, buf, n, ms);
    return ssl::recvn(_s[-1], buf, n, ms);
}

int Client::send(const void* buf, int n, int ms) {
    if (!_use_ssl) return co::send(_fd, buf, n, ms);
    return ssl::send(_s[-1], buf, n, ms);
}

bool Client::connect(int ms) {
    if (this->connected()) return true;

    fastring port = str::from(_port);
    struct addrinfo* info = 0;
    int r = getaddrinfo(_ip, port.c_str(), NULL, &info);
    if (r != 0) goto end;

    CHECK_NOTNULL(info);
    _fd = (int) co::tcp_socket(info->ai_family);
    if (_fd == -1) {
        ELOG << "connect to " << _ip << ':' << _port << " failed: " << co::strerror();
        goto end;
    }

    r = co::connect(_fd, info->ai_addr, (int)info->ai_addrlen, ms);
    if (r != 0) {
        ELOG << "connect to " << _ip << ':' << _port << " failed: " << co::strerror();
        goto end;
    }

    co::set_tcp_nodelay(_fd);
    if (_use_ssl) {
        if ((_s[-2] = ssl::new_client_ctx()) == NULL) goto new_ctx_err;
        if ((_s[-1] = ssl::new_ssl(_s[-2])) == NULL) goto new_ssl_err;
        if (ssl::set_fd(_s[-1], _fd) != 1) goto set_fd_err;
        if (ssl::connect(_s[-1], ms) != 1) goto connect_err;
    }

    if (info) freeaddrinfo(info);
    return true;

  new_ctx_err:
    ELOG << "ssl connect new client contex failed: " << ssl::strerror();
    goto end;
  new_ssl_err:
    ELOG << "ssl connect new SSL failed: " << ssl::strerror();
    goto end;
  set_fd_err:
    ELOG << "ssl connect set fd " << _fd << " failed: " << ssl::strerror(_s[-1]);
    goto end;
  connect_err:
    ELOG << "ssl connect failed: " << ssl::strerror(_s[-1]);
    goto end;
  end:
    this->disconnect();
    if (info) freeaddrinfo(info);
    return false;
}

void Client::disconnect() {
    if (_fd != -1) {
        if (_use_ssl) {
            if (_s[-1]) { ssl::free_ssl(_s[-1]); _s[-1] = 0; }
            if (_s[-2]) { ssl::free_ctx(_s[-2]); _s[-2] = 0; }
        }
        co::close(_fd); _fd = -1;
    }
}

const char* Client::strerror() const {
    return !_use_ssl ? co::strerror() : ssl::strerror(_s[-1]);
}

} // tcp
