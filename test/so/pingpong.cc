// build:
//   xmake -b pingpong
//
// server:
//   xmake r pingpong                          # 127.0.0.1:9988
//   xmake r pingpong ip=0.0.0.0 port=7777     # 0.0.0.0:7777
//   xmake r pingpong ip=::                    # :::9988  (ipv6)
//   
// client:
//   xmake r pingpong -c                       # -> 127.0.0.1:9988
//   xmake r pingpong -c ip=::1                # -> ::1:9988
//
// special notes:
//   For ipv6 link-local address, we have to specify the network interface:
//   xmake r pingpong ip=fe80::a00:27ff:fea7:a888%eth0
//   xmake r pingpong ip=fe80::a00:27ff:fea7:a888%eth0 -c

#include "co/all.h"

DEF_string(ip, "127.0.0.1", "ip");
DEF_int32(port, 9988, "port");
DEF_bool(c, false, "run as client");

class PingPongServer : public tcp::Server {
  public:
    PingPongServer(const char* ip, int port)
        : tcp::Server(ip, port) {
    }

    virtual ~PingPongServer() = default;

    virtual void on_connection(so::Connection* conn);
};

void PingPongServer::on_connection(so::Connection* conn) {
    std::unique_ptr<so::Connection> c(conn);
    sock_t fd = c->fd;
    co::set_tcp_keepalive(fd);
    co::set_tcp_nodelay(fd);
    
    char buf[8] = { 0 };

    COUT << "new connection: " << *conn;

    while (true) {
        int r = co::recv(fd, buf, 8);
        if (r == 0) {         /* client close the connection */
            COUT << "connection closed: " << *conn;
            co::close(fd);
            break;
        } else if (r == -1) { /* error */
            co::reset_tcp_socket(fd, 3000);
            break;
        } else {
            COUT << "recv " << fastring(buf, r);
            COUT << "send pong";
            r = co::send(fd, "pong", 4);
            if (r == -1) {
                COUT << "server send error: " << co::strerror();
                co::reset_tcp_socket(fd, 3000);
                break;
            }
        }
    }
}

SyncEvent kEvent;

void client_fun(void* p) {
    tcp::Client c(FLG_ip.c_str(), FLG_port);
    if (!c.connect(1000)) {
        COUT << "connect to server " << FLG_ip << ':' << FLG_port
             << " failed, error: " << co::strerror();
    } else {
        COUT << "connect to server " << FLG_ip << ':' << FLG_port << " success";
        int r = c.send("ping", 4);
        if (r == -1) {
            COUT << "send ping failed: " << co::strerror();
        } else if (r == 0) {
            COUT << "server close the connection..";
        } else {
            COUT << "send ping";
            char buf[8];
            r = c.recv(buf, 8);
            if (r == -1) {
                COUT << "recv error: " << co::strerror();
            } else if (r == 0) {
                COUT << "server close the connection..";
            } else {
                COUT << "recv " << fastring(buf, r);
            }
        }
    }

    SyncEvent* ev = (SyncEvent*) p;
    ev->signal();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    if (!FLG_c) {
        PingPongServer serv(FLG_ip.c_str(), FLG_port);
        serv.start();
        COUT << "pingpong server start: " << FLG_ip << ":" << FLG_port;
        while (true) sleep::sec(1024);
    } else {
        go(client_fun, (void*)&kEvent);
        kEvent.wait();
    }

    return 0;
}
