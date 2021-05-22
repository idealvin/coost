#include "co/all.h"

DEF_string(ip, "127.0.0.1", "ip");
DEF_int32(port, 9988, "port");

void on_connection(tcp::Connection* conn) {
    std::unique_ptr<tcp::Connection> c(conn);
    char buf[8] = { 0 };

    while (true) {
        int r = conn->recv(buf, 8);
        if (r == 0) {         /* client close the connection */
            conn->close();
            break;
        } else if (r == -1) { /* error */
            conn->reset(3000);
            break;
        } else {
            LOG << "server recv " << fastring(buf, r);
            LOG << "server send pong";
            r = conn->send("pong", 4);
            if (r == -1) {
                LOG << "server send error: " << co::strerror();
                conn->reset(3000);
                break;
            }
        }
    }
}

void client_fun() {
    tcp::Client c(FLG_ip.c_str(), FLG_port);
    if (!c.connect(3000)) {
        LOG << "failed to connect to server " << FLG_ip << ':' << FLG_port
            << " error: " << co::strerror();
        return;
    }

    char buf[8] = { 0 };

    while (true) {
        LOG << "client send ping";
        int r = c.send("ping", 4);
        if (r == -1) {
            LOG << "client send error: " << co::strerror();
            break;
        }

        r = c.recv(buf, 8);
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

    c.disconnect();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();
    FLG_cout = true;

    tcp::Server s;
    s.on_connection(on_connection);
    s.start(FLG_ip.c_str(), FLG_port);

    sleep::ms(32);
    go(client_fun);

    while (true) sleep::sec(1024);

    return 0;
}
