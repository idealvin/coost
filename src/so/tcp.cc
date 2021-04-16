#include "co/so/tcp.h"
#include "co/log.h"
#include "co/str.h"

namespace so {
namespace tcp {

void Server::loop() {
    sock_t fd, connfd;

    union {
        struct sockaddr_in  v4;
        struct sockaddr_in6 v6;
    } addr; // use union here to support both ipv4 and ipv6

    int addrlen = sizeof(addr);

    do {
        fastring port = str::from(_port);
        struct addrinfo* info = 0;
        int r = getaddrinfo(_ip.c_str(), port.c_str(), NULL, &info);
        CHECK_EQ(r, 0) << "invalid ip address: " << _ip << ':' << _port;
        CHECK(info != NULL);

        fd = co::tcp_socket(info->ai_family);
        CHECK_NE(fd, (sock_t)-1) << "create socket error: " << co::strerror();
        co::set_reuseaddr(fd);

        // turn off IPV6_V6ONLY
        if (info->ai_family == AF_INET6) {
            int on = 0; 
            co::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
        }

        r = co::bind(fd, info->ai_addr, (int)info->ai_addrlen);
        CHECK_EQ(r, 0) << "bind (" << _ip << ':' << _port << ") failed: " << co::strerror();

        r = co::listen(fd, 1024);
        CHECK_EQ(r, 0) << "listen error: " << co::strerror();

        freeaddrinfo(info);
    } while (0);

    while (true) {
        addrlen = sizeof(addr);
        connfd = co::accept(fd, &addr, &addrlen);
        if (unlikely(connfd == (sock_t)-1)) {
            WLOG << "accept error: " << co::strerror();
            continue;
        }

        Connection* conn = new Connection;
        conn->fd = connfd;
        if (addrlen == sizeof(sockaddr_in)) {
            conn->peer.reserve(24);
            conn->peer << co::ip_str(&addr.v4) << ':' << ntoh16(addr.v4.sin_port);
        } else {
            conn->peer.reserve(72);
            conn->peer << co::ip_str(&addr.v6) << ':' << ntoh16(addr.v6.sin6_port);
        }

        typedef void (Server::*F)(void*);
        go((F)&Server::on_connection, this, conn);
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
        if (_sched_id != -1) { CHECK_EQ(_sched_id, co::scheduler_id()); _sched_id = -1; }
    }
}
} // tcp
} // so
