#include "co/flag.h"
#include "co/print.h"
#include "co/co.h"
#include "co/tcp.h"
#include "co/time.h"

DEF_string(ip, "127.0.0.1", "server ip");
DEF_int32(port, 9988, "server port");
DEF_int32(c, 1, "client num");

void conn_cb(sock_t fd) {
    char buf[8] = { 0 };

    while (true) {
        int r = co::recv(fd, buf, 8);
        if (r == 0) {         /* client close the connection */
            co::println("client close the connection: ", fd);
            co::close(fd);
            break;
        } else if (r < 0) { /* error */
            co::reset_tcp_socket(fd, 1000);
            break;
        } else {
            co::println("server recv: ", co::string(buf, r));
            co::println("server send: pong");
            r = co::send(fd, "pong", 4);
            if (r <= 0) {
                co::println("server send error: ", co::strerror());
                co::reset_tcp_socket(fd, 1000);
                break;
            }
        }
    }
}

void client_fun() {
    co::tcp_client c(FLG_ip.c_str(), (uint16)FLG_port);
    if (!c.connect(3000)) return;

    char buf[8] = { 0 };

    for (int i = 0; i < 3; ++i) {
        co::println("client send: ping");
        int r = c.send("ping", 4);
        if (r <= 0) {
            co::println("client send error: ", co::strerror());
            break;
        }

        r = c.recv(buf, 8);
        if (r < 0) {
            co::println("client recv error: ", co::strerror());
            break;
        } else if (r == 0) {
            co::println("server close the connection");
            break;
        } else {
            co::println("client recv: ", co::string(buf, r), '\n');
            co::sleep(500);
        }
    }

    c.disconnect();
}


co::pool* g_pool;

// we don't need to close the connection manually with co::Pool.
void client_with_pool() {
    co::pool_guard<co::tcp_client> c(*g_pool);
    if (!c->connect(3000)) return;

    char buf[8] = { 0 };

    for (int i = 0; i < 3; ++i) {
        co::println("client send: ping");
        int r = c->send("ping", 4);
        if (r <= 0) {
            co::println("client send error: ", co::strerror());
            break;
        }

        r = c->recv(buf, 8);
        if (r < 0) {
            co::println("client recv error: ", co::strerror());
            break;
        } else if (r == 0) {
            co::println("server close the connection");
            break;
        } else {
            co::println("client recv: ", co::string(buf, r), '\n');
            co::sleep(500);
        }
    }
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    flag::set_value("also_log2console", "true");

    g_pool = co::make_static<co::pool>(
        []() {
            return (void*) new co::tcp_client(FLG_ip.c_str(), (uint16)FLG_port);
        },
        [](void* p) { delete (co::tcp_client*) p; }
    );

    co::tcp_server s("0.0.0.0", FLG_port);
    s.on_connection(conn_cb).start();

    co::sleep(32);

    if (FLG_c > 1) {
        for (int i = 0; i < FLG_c; ++i) {
            go(client_with_pool);
        }
    } else {
        go(client_fun);
    }

    while (true) time::sleep(10000);

    return 0;
}
