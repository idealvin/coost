#include "co/so/tcp.h"
#include "co/log.h"
#include "co/str.h"

namespace tcp {

struct ServerParam {
    ServerParam(const char* ip, int port, std::function<void(sock_t)>&& on_connection)
        : ip((ip && *ip) ? ip : "0.0.0.0"), port(str::from(port)), 
          on_connection(std::move(on_connection)) {
    }

    fastring ip;
    fastring port;
    sock_t fd;
    sock_t connfd;
    std::function<void(sock_t)> on_connection;

    // use union here to support both ipv4 and ipv6
    union {
        struct sockaddr_in  v4;
        struct sockaddr_in6 v6;
    } addr;
    int addrlen;
};

void Server::start(const char* ip, int port) {
    CHECK(_on_connection != NULL) << "connection callback not set..";
    ServerParam* p = new ServerParam(ip, port, std::move(_on_connection));
    go(&Server::loop, (void*)p);
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
        go(&p->on_connection, p->connfd);
    }
}

bool Client::connect(int ms) {
    if (this->connected()) return true;

    fastring port = str::from(_port);
    struct addrinfo* info = 0;
    int r = getaddrinfo(_ip.c_str(), port.c_str(), NULL, &info);
    if (r != 0) goto err;

    CHECK_NOTNULL(info);
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

    co::set_tcp_nodelay(_fd);
    if (info) freeaddrinfo(info);
    return true;

  err:
    this->disconnect();
    if (info) freeaddrinfo(info);
    return false;
}

} // tcp
