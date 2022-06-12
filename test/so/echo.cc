#include "co/all.h"

DEF_string(ip, "127.0.0.1", "ip");
DEF_int32(port, 9988, "port");
DEF_int32(c, 0, "client num");
DEF_int32(l, 4096, "message length");
DEF_int32(t, 60, "test time in seconds");

void conn_cb(tcp::Connection conn) {
    fastream buf(FLG_l);

    while (true) {
        int r = conn.recvn(&buf[0], FLG_l);
        if (r == 0) {         /* client close the connection */
            conn.close();
            break;
        } else if (r < 0) { /* error */
            conn.reset(3000);
            break;
        } else {
            r = conn.send(buf.data(), FLG_l);
            if (r <= 0) {
                conn.reset(3000);
                break;
            }
        }
    }
}

bool g_stop = false;
struct Count {
    uint32 r;
    uint32 s;
};
Count* g_count;

void client_fun(int i) {
    tcp::Client c(FLG_ip.c_str(), FLG_port, false);
    if (!c.connect(3000)) return;

    fastring buf(FLG_l, 'x');
    auto& count = g_count[i];

    while (!g_stop) {
        int r = c.send(buf.data(), FLG_l);
        if (r <= 0) {
            break;
        }
        ++count.s;

        r = c.recvn(&buf[0], FLG_l);
        if (r < 0) {
            break;
        } else if (r == 0) {
            LOG << "server close the connection";
            break;
        } 
        ++count.r;
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
        g_count = (Count*) co::zalloc(sizeof(Count) * FLG_c);
        for (int i = 0; i < FLG_c; ++i) {
            go(client_fun, i);
        }

        sleep::sec(FLG_t);
        atomic_store(&g_stop, true);
        sleep::sec(3);

        size_t rsum = 0;
        size_t ssum = 0;
        for (int i = 0; i < FLG_c; ++i) {
            rsum += g_count[i].r;
            ssum += g_count[i].s;
        }

        COUT << "Speed: "
             << (ssum / FLG_t) << " request/sec, "
             << (rsum / FLG_t) << " response/sec";
        COUT << "Requests: " << ssum;
        COUT << "Responses: " << rsum;
    }

    return 0;
}
