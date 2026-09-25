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
        log::error("connect failed, invalid addr: ", _server_host, ':', _server_port);
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
        : _addr(), _connfd((sock_t)-1), _fd((sock_t)-1) {
        _x.state = -1;
        _x.conn_num = 0;
        _ip = (ip && *ip) ? ip : "0.0.0.0";
        _port = port;
    }

    ~tcp_server_impl() { this->stop(); }

    void start();
    void stop();
    void loop();
    void on_connection(sock_t fd);

    uint32 conn_num() { return atomic_load(&_x.conn_num, mo_relaxed); }

    union {
        struct {
            int32 state; // init: -1, 0: started, 1: stopping, 2: stopped
            uint32 conn_num;
        } _x;
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
    atomic_dec(&_x.conn_num, mo_relaxed);
}

void tcp_server_impl::start() {
    runtime_assert(_conn_cb, "connection callback not set");
    go(&tcp_server_impl::loop, this);
}

void tcp_server_impl::stop() {
    const int32 state = atomic_cas(&_x.state, 0, 1, mo_acq_rel, mo_acquire);
    if (state == 0) {
        go([](void* p) {
            auto serv = (tcp_server_impl*)p;
            const char* ip = serv->_ip;
            if (strcmp(ip, "0.0.0.0") == 0 || strcmp(ip, "::") == 0) {
                ip = "127.0.0.1";
            }

            auto fd = co::tcp_socket();
            runtime_assert(fd != (sock_t)-1);
            co::sockaddr addr(ip, serv->_port);
            co::connect(fd, addr);
            co::close(fd);
        }, this);
        while (atomic_load(&_x.state, mo_acquire) != 2) co::sleep(1);

    } else if (state == 1) {
        while (atomic_load(&_x.state, mo_acquire) != 2) co::sleep(1);
    }
}

void tcp_server_impl::loop() {
    _fd = co::tcp_server_socket(_ip, _port, 4096);
    runtime_assert(_fd != (sock_t)-1);
    atomic_store(&_x.state, 0, mo_release);

    log::info("server start: ", _ip, ':', _port);

    while (atomic_load(&_x.state, mo_acquire) != 1) {
        _connfd = co::accept(_fd, &_addr);
        if (atomic_load(&_x.state, mo_acquire) == 1) {
            if (_connfd != (sock_t)-1) co::reset_tcp_socket(_connfd);
            break;
        }

        if (_connfd != (sock_t)-1) {
            const uint32 n = atomic_inc(&_x.conn_num, mo_relaxed);
            log::info(
                "server(", _ip, ':', _port, ") accept connection: ", _addr,
                ", connfd: ", _connfd, ", conn num: ", n
            );
            go(&tcp_server_impl::on_connection, this, _connfd);
        } else {
            log::warn("server(", _ip, ':', _port, ") accept error: ", co::strerror());
        }
    }

    co::close(_fd);
    atomic_store(&_x.state, 2, mo_release);
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
        co::free(_p, sizeof(tcp_server_impl));
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

void tcp_server::stop() {
    static_cast<tcp_server_impl*>(_p)->stop();
}

uint32 tcp_server::conn_num() {
    return static_cast<tcp_server_impl*>(_p)->conn_num();
}

} // tcp
