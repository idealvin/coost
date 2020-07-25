#include "co/all.h"

DEF_string(ip, "127.0.0.1", "http server ip or dns");
DEF_int32(port, 80, "http server port");
DEF_int32(n, 1, "req num");

SyncEvent ev;

void fun() {
    http::Client cli(FLG_ip.c_str(), FLG_port);
 
    for (int i = 0; i < FLG_n; ++i) {
        fastring s = cli.get("/");
        COUT << "content length: " << s.size();
    }
   
    for (int i = 0; i < FLG_n; ++i) {
        http::Req req;
        http::Res res;
        req.set_version_http10();
        req.set_method_get();
        req.set_url("/");
        cli.call(req, res);
        COUT << "content length: " << res.body_len();
    }

    ev.signal();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    go(fun);

    ev.wait();
    co::stop();
    return 0;
}
