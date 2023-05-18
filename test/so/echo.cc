#include "co/all.h"

DEF_string(h, "127.0.0.1", "server ip");
DEF_int32(p, 9988, "server port");
DEF_int32(c, 128, "connection number");
DEF_int32(l, 1024, "message length");
DEF_int32(t, 10, "test time in seconds");
DEF_bool(s, false, "run as server if true");

void conn_cb(tcp::Connection conn) {
    fastream buf(FLG_l);

    while (true) {
        int r = conn.recvn(&buf[0], FLG_l);
        if (r > 0) {
            r = conn.send(buf.data(), FLG_l);
            if (r <= 0) {
                conn.reset(3000);
                break;
            }
        } else if (r == 0) { /* client close the connection */
            conn.close();
            break;
        } else { /* error */
            conn.reset(3000);
            break;
        }
    }
}

bool g_stop = false;
struct Count {
    uint32 r;
    uint32 s;
    char x[56];
};
Count* g_count;
co::wait_group g_wg;

void client_fun(int i) {
    tcp::Client c(FLG_h.c_str(), FLG_p, false);
    defer(
        c.close();
        g_wg.done();
    );
    if (!c.connect(3000)) {
        co::print("connect to server failed..");
        return;
    }

    fastring buf(FLG_l, '\0');
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
            co::print("server close the connection");
            break;
        } 
        ++count.r;
    }
}

int main(int argc, char** argv) {
    flag::set_value("co_sched_num", "1");
    FLG_help << "usage: \n"
             << "\techo -s            # run echo server\n"
             << "\techo -c 128 -t 20  # run echo client, 128 connection, 20 seconds\n";
    flag::parse(argc, argv);

    if (FLG_s) {
        tcp::Server().on_connection(conn_cb).start("0.0.0.0", FLG_p);
        while (true) sleep::sec(1024);
    } else {
        g_count = (Count*) co::zalloc(sizeof(Count) * FLG_c);
        g_wg.add(FLG_c);
        go([]() {
            co::sleep(FLG_t * 1000);
            atomic_store(&g_stop, true);
        });
        for (int i = 0; i < FLG_c; ++i) {
            go(client_fun, i);
        }
        g_wg.wait();

        size_t rsum = 0;
        size_t ssum = 0;
        for (int i = 0; i < FLG_c; ++i) {
            rsum += g_count[i].r;
            ssum += g_count[i].s;
        }

        co::print("server: ", FLG_h, ":", FLG_p);
        co::print("connection num: ", FLG_c, ", msg len: ", FLG_l, ", time: ", FLG_t, " seconds");
        co::print("speed: ",
            (ssum / FLG_t), " request/sec, ",
            (rsum / FLG_t), " response/sec"
        );
        co::print("requests: ", ssum);
        co::print("responses: ", rsum);
    }

    return 0;
}
