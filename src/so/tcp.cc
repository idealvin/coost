#include "co/so/tcp.h"
#include "co/log.h"
#include "co/str.h"

namespace tcp {

struct ServerParam {
    fastring ip;
    fastring port;
    sock_t fd;
    sock_t connfd;

    // use union here to support both ipv4 and ipv6
    union {
        struct sockaddr_in  v4;
        struct sockaddr_in6 v6;
    } addr;
    int addrlen;
};

void Server::start(const char* ip, int port) {
    CHECK(_on_connection != NULL) << "connection callback not set..";
    ServerParam* p = new ServerParam();
    p->ip = ((ip && *ip) ? ip : "0.0.0.0");
    p->port = str::from(port);
    go(&Server::loop, this, (void*)p);
}

void Server::loop(void* arg) {
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

    LOG << "server_" << p->fd << " start: " << p->ip << ':' << p->port;
    while (true) {
        p->addrlen = sizeof(p->addr);
        p->connfd = co::accept(p->fd, &p->addr, &p->addrlen);
        if (unlikely(p->connfd == (sock_t)-1)) {
            WLOG << "server_" << p->fd << " accept error: " << co::strerror();
            continue;
        }

        DLOG << "server_" << p->fd << " accept new connection: "
             << co::to_string(&p->addr, p->addrlen) << ", connfd: " << p->connfd;
        go(&_on_connection, p->connfd);
    }
}

bool Client::connect(int ms) {
    if (this->connected()) return true;

    fastring port = str::from(_port);
    struct addrinfo* info = 0;
    int r = getaddrinfo(_ip.c_str(), port.c_str(), NULL, &info);
    CHECK_EQ(r, 0) << "invalid ip address: " << _ip << ':' << _port;
    CHECK(info != NULL);

    _fd = co::tcp_socket(info->ai_family);
    if (_fd == (sock_t)-1) {
        ELOG << "connect to " << _ip << ':' << _port << " failed, create socket error: " << co::strerror();
        goto err;
    }

    r = co::connect(_fd, info->ai_addr, (int)info->ai_addrlen, ms);
    if (r == -1) {
        ELOG << "connect to " << _ip << ':' << _port << " failed: " << co::strerror();
        goto err;
    }

    _sched_id = co::scheduler_id();
    co::set_tcp_nodelay(_fd);
    freeaddrinfo(info);
    return true;

  err:
    if (_fd != (sock_t)-1) { co::close(_fd); _fd = (sock_t)-1; }
    if (_sched_id != -1) { _sched_id = -1; }
    freeaddrinfo(info);
    return false;
}

void Client::disconnect() {
    if (this->connected()) {
        if (_fd != (sock_t)-1) { co::close(_fd); _fd = (sock_t)-1; }
        if (_sched_id != -1) {
            CHECK_EQ(_sched_id, co::scheduler_id());
            _sched_id = -1;
        }
    }
}

} // tcp
