#pragma once

#include "sock.h"
#include <functional>

namespace co {

struct tcp_client {
    tcp_client(const char* server_host, uint16 server_port) {
        _server_host = (server_host && *server_host) ? server_host : "127.0.0.1";
        _server_port = server_port;
        _fd = (sock_t)-1;
    }

    tcp_client(const tcp_client& c) {
        _server_host = c._server_host;
        _server_port = c._server_port;
        _fd = (sock_t)-1;
    }

    ~tcp_client() { this->disconnect(); }

    tcp_client(tcp_client&&) = delete;
    void operator=(const tcp_client& c) = delete;
    void operator=(tcp_client&& c) = delete;

    bool connected() const { return _fd != (sock_t)-1; }

    bool connect(int ms);
    void disconnect();
    void close() { this->disconnect(); }

    int recv(void* buf, int n, int ms=-1) {
        return co::recv(_fd, buf, n, ms);
    }

    int recvn(void* buf, int n, int ms=-1) {
        return co::recvn(_fd, buf, n, ms);
    }

    int send(const void* buf, int n, int ms=-1) {
        return co::send(_fd, buf, n, ms);
    }

    const char* _server_host;
    uint16 _server_port;
    sock_t _fd;
};

struct tcp_server {
    tcp_server(const char* ip, uint16 port);
    ~tcp_server();

    tcp_server(const tcp_server&) = delete;
    tcp_server(tcp_server&&) = delete;
    void operator=(const tcp_server&) = delete;
    void operator=(tcp_server&&) = delete;
    
    using conn_cb_t = std::function<void(sock_t)>;

    // set a callback to handle incoming connections
    tcp_server& on_connection(conn_cb_t&& cb);

    tcp_server& on_connection(const conn_cb_t& cb) {
        return this->on_connection(conn_cb_t(cb));
    }

    void start();

    uint32 conn_num();

    void* _p;
};

} // co

namespace tcp {

using client = co::tcp_client;
using server = co::tcp_server;

} // tcp
