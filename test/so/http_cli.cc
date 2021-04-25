#include "co/all.h"

DEF_string(serv, "127.0.0.1:80", "url of the http server");
DEF_int32(n, 1, "req num");

SyncEvent ev;

void fun() {
    http::Client cli(FLG_serv.c_str());
 
    for (int i = 0; i < FLG_n; ++i) {
        auto& res = cli.get("/");
        COUT << "content length: " << res.body_size();
    }
   
    for (int i = 0; i < FLG_n; ++i) {
        http::Req req;
        http::Res res;
        req.set_version_http10();
        req.set_method_get();
        req.set_url("/");
        cli.call(req, res);
        COUT << "content length: " << res.body_size();
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
