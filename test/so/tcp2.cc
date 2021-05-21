#include "co/all.h"

DEF_string(ip, "127.0.0.1", "ip");
DEF_int32(port, 9988, "port");

struct Connection {
    sock_t fd;   // conn fd
    fastring ip; // peer ip
    int port;    // peer port
};

void on_connection(void* p) {
    std::unique_ptr<Connection> conn((Connection*)p);
    sock_t fd = conn->fd;
    co::set_tcp_keepalive(fd);
    co::set_tcp_nodelay(fd);

    char buf[8] = { 0 };

    while (true) {
        int r = co::recv(fd, buf, 8);
        if (r == 0) {         /* client close the connection */
            co::close(fd);
            break;
        } else if (r == -1) { /* error */
            co::reset_tcp_socket(fd, 3000);
            break;
        } else {
            LOG << "server recv " << fastring(buf, r);
            LOG << "server send pong";
            r = co::send(fd, "pong", 4);
            if (r == -1) {
                LOG << "server send error: " << co::strerror();
                co::reset_tcp_socket(fd, 3000);
                break;
            }
        }
    }
}

void server_fun() {
    sock_t fd = co::tcp_socket();
    co::set_reuseaddr(fd);

    sock_t connfd;
    int addrlen = sizeof(sockaddr_in);
    struct sockaddr_in addr;
    co::init_ip_addr(&addr, FLG_ip.c_str(), FLG_port);

    co::bind(fd, &addr, sizeof(addr));
    co::listen(fd, 1024);

    while (true) {
        addrlen = sizeof(sockaddr_in);
        connfd = co::accept(fd, &addr, &addrlen);
        if (connfd == -1) continue;

        Connection* conn = new Connection;
        conn->fd = connfd;
        conn->ip = co::ip_str(&addr);
        conn->port = ntoh16(addr.sin_port);

        // create a new coroutine for this connection
        LOG << "server accept new connection: " << conn->ip << ":" << conn->port;
        co::go(on_connection, conn);
    }
}

void client_fun() {
    sock_t fd = co::tcp_socket();

    struct sockaddr_in addr;
    co::init_ip_addr(&addr, FLG_ip.c_str(), FLG_port);

    co::connect(fd, &addr, sizeof(addr), 3000);
    co::set_tcp_nodelay(fd);

    char buf[8] = { 0 };

    while (true) {
        LOG << "client send ping";
        int r = co::send(fd, "ping", 4);
        if (r == -1) {
            LOG << "client send error: " << co::strerror();
            break;
        }

        r = co::recv(fd, buf, 8);
        if (r == -1) {
            LOG << "client recv error: " << co::strerror();
            break;
        } else if (r == 0) {
            LOG << "server close the connection";
            break;
        } else {
            LOG << "client recv " << fastring(buf, r) << '\n';
            co::sleep(3000);
        }
    }

    co::close(fd);
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();
    FLG_cout = true;

    go(server_fun);
    sleep::ms(32);
    go(client_fun);

    while (true) sleep::sec(1024);

    return 0;
}
