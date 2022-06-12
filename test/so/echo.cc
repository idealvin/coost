#include "co/all.h"

DEF_string(ip, "127.0.0.1", "ip");
DEF_int32(port, 9988, "port");
DEF_int32(c, 0, "client num");
DEF_int32(b, 4096, "body size");
DEF_int32(n, 1000000, "package number for each client");

void conn_cb(tcp::Connection conn) {
    fastream buf(FLG_b);

    while (true) {
        int r = conn.recvn(&buf[0], FLG_b);
        if (r == 0) {         /* client close the connection */
            conn.close();
            break;
        } else if (r < 0) { /* error */
            conn.reset(3000);
            break;
        } else {
            r = conn.send(buf.data(), FLG_b);
            if (r <= 0) {
                conn.reset(3000);
                break;
            }
        }
    }
}

void client_fun() {
    tcp::Client c(FLG_ip.c_str(), FLG_port, false);
    if (!c.connect(3000)) return;

    fastring buf(FLG_b, 'x');

    for (int i = 0; i < FLG_n; ++i) {
        int r = c.send(buf.data(), FLG_b);
        if (r <= 0) {
            break;
        }

        r = c.recvn(&buf[0], FLG_b);
        if (r < 0) {
            break;
        } else if (r == 0) {
            LOG << "server close the connection";
            break;
        } 
    }

    c.close();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);

    if (FLG_c <= 0) {
        tcp::Server().on_connection(conn_cb).start(
            FLG_ip.c_str(), FLG_port 
        );
        while (true) sleep::sec(102400);
    } else {
        for (int i = 0; i < FLG_c; ++i) {
            go(client_fun);
        }
        while (true) sleep::sec(102400);
    }

    return 0;
}
