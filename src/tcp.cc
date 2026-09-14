#include "co/tcp.h"
#include "co/co.h"
#include "co/log.h"
#include "co/atomic.h"

namespace co {

bool tcp_client::connect(int ms) {
    if (this->connected()) return true;

    int r;
    co::sockaddr addr(_server_host, _server_port);
    if (!addr.valid()) {
        log::error(
            "connect failed, invalid server addr: ", _server_host, ':', _server_port
        );
        goto end;
    }

    _fd = co::tcp_socket(addr.af());
    if (_fd == (sock_t)-1) goto end;

    r = co::connect(_fd, addr, ms);
    if (r != 0) {
        log::error(
            "connect to ", _server_host, ':', _server_port, " failed: ", co::strerror()
        );
        goto end;
    }

    co::set_tcp_nodelay(_fd);
    return true;

end:
    this->disconnect();
    return false;
}

void tcp_client::disconnect() {
    if (this->connected()) {
        co::close(_fd);
        _fd = (sock_t)-1;
    }
}

struct __cacheline_aligned tcp_server_impl {
    using conn_cb_t = tcp_server::conn_cb_t;

    tcp_server_impl(const char* ip, uint16 port)
        : _conn_num(0), _addr(), _fd((sock_t)-1) {
        _ip = (ip && *ip) ? ip : "0.0.0.0";
        _port = port;
    }

    ~tcp_server_impl() {
        if (_fd != (sock_t)-1) co::close(_fd);
    }

    void start();
    void loop();
    void on_connection(sock_t fd);

    uint32 conn_num() { return co::atomic_load(&_conn_num, co::mo_relaxed); }

    union {
        uint32 _conn_num;
        char _buf[co::cache_line_size];
    };

    co::sockaddr _addr;
    sock_t _connfd;
    sock_t _fd;
    const char* _ip;
    uint16 _port;
    conn_cb_t _conn_cb;
};

void tcp_server_impl::on_connection(sock_t fd) {
    co::set_tcp_keepalive(fd);
    co::set_tcp_nodelay(fd);
    _conn_cb(fd);
    co::atomic_dec(&_conn_num, co::mo_relaxed);
}

void tcp_server_impl::start() {
    runtime_assert(_conn_cb, "connection callback not set");
    go(&tcp_server_impl::loop, this);
}

void tcp_server_impl::loop() {
    _fd = co::tcp_server_socket(_ip, _port, 4096);
    runtime_assert(_fd != (sock_t)-1);

    log::info("server start: ", _ip, ':', _port);

    while (true) {
        _connfd = co::accept(_fd, &_addr);
        if (_connfd != (sock_t)-1) {
            const uint32 n = co::atomic_inc(&_conn_num, co::mo_relaxed);
            log::info(
                "server(", _ip, ':', _port, ") accept connection: ", _addr,
                ", connfd: ", _connfd, ", conn num: ", n
            );
            go(&tcp_server_impl::on_connection, this, _connfd);
        } else {
            log::warn("server(", _ip, ':', _port, ") accept error: ", co::strerror());
        }
    }
}

tcp_server::tcp_server(const char* ip, uint16 port) {
    _p = co::alloc(sizeof(tcp_server_impl), co::cache_line_size);
    runtime_assert(_p);
    new (_p) tcp_server_impl(ip, port);
}

tcp_server::~tcp_server() {
    if (_p) {
        const auto p = static_cast<tcp_server_impl*>(_p);
        p->~tcp_server_impl();
        _p = nullptr;
    }
}

tcp_server& tcp_server::on_connection(conn_cb_t&& cb) {
    static_cast<tcp_server_impl*>(_p)->_conn_cb = std::move(cb);
    return *this;
}

void tcp_server::start() {
    static_cast<tcp_server_impl*>(_p)->start();
}

uint32 tcp_server::conn_num() {
    return static_cast<tcp_server_impl*>(_p)->conn_num();
}

} // tcp
