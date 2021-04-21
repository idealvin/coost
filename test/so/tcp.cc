#include "co/all.h"

DEF_string(ip, "127.0.0.1", "ip");
DEF_int32(port, 9988, "port");

void on_connection(sock_t fd) {
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
            COUT << "server recv " << fastring(buf, r);
            COUT << "server send pong";
            r = co::send(fd, "pong", 4);
            if (r == -1) {
                COUT << "server send error: " << co::strerror();
                co::reset_tcp_socket(fd, 3000);
                break;
            }
        }
    }
}

void client_fun() {
    tcp::Client c(FLG_ip.c_str(), FLG_port);
    if (!c.connect(3000)) {
        COUT << "failed to connect to server " << FLG_ip << ':' << FLG_port;
        return;
    }

    char buf[8] = { 0 };

    while (true) {
        COUT << "client send ping";
        int r = c.send("ping", 4);
        if (r == -1) {
            COUT << "client send error: " << co::strerror();
            break;
        }

        r = c.recv(buf, 8);
        if (r == -1) {
            COUT << "client recv error: " << co::strerror();
            break;
        } else if (r == 0) {
            COUT << "server close the connection";
            break;
        } else {
            COUT << "client recv " << fastring(buf, r) << '\n';
            co::sleep(3000);
        }
    }

    c.disconnect();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);

    tcp::Server s;
    s.on_connection(on_connection);
    s.start(FLG_ip.c_str(), FLG_port);

    sleep::ms(32);
    go(client_fun);

    while (true) sleep::sec(1024);

    return 0;
}
