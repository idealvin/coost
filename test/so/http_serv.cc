#include "co/flag.h"
#include "co/log.h"
#include "co/so.h"
#include "co/time.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    http::Server serv("127.0.0.1", 80);

    serv.on_req(
        [](const http::Req& req, http::Res& res) {
            if (req.is_method_get()) {
                if (req.url() == "/hello") {
                    res.set_status(200);
                    res.set_body("hello world");
                } else {
                    res.set_status(404);
                }
            } else {
                res.set_status(501);
            }
        }
    );

    serv.start();

    while (true) sleep::sec(1024);
    return 0;
}
