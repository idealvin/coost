#include "co/flag.h"
#include "co/log.h"
#include "co/so.h"
#include "co/time.h"

DEF_string(ip, "127.0.0.1", "http server ip");
DEF_int32(port, 80, "http server port");

void fun() {
    http::Client cli(FLG_ip.c_str(), FLG_port);
    
    for (int i = 0; i < 3; ++i) {
        http::Req req;
        http::Res res;
        req.set_method_get();
        req.set_url("/");

        COUT << "send req: " << req.dbg();
        cli.call(req, res);

        COUT << "recv res: " << res.dbg();
        COUT << "body: " << res.body();
    }
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    go(fun);

    sleep::sec(3);
    return 0;
}
