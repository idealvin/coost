// http server support both ipv4 and ipv6
//
// build:
//   xmake -b http_serv
//
// start http server:
//   xmake r http_serv                              # 0.0.0.0:80
//   xmake r http_serv -ip 127.0.0.1 -port 7777     # 127.0.0.1:7777
//   xmake r http_serv -ip ::                       # [::]:80  (ipv6)
//   xmake r http_serv -key "key.pem" -ca "ca.pem"  # https://0.0.0.0:443
//   
// NOTE:
//   For ipv6 link-local address, we have to specify the network interface:
//   xmake r http_serv ip=fe80::a00:27ff:fea7:a888%eth0

#include "co/flag.h"
#include "co/log.h"
#include "co/http.h"
#include "co/time.h"

DEF_string(ip, "0.0.0.0", "http server ip");
DEF_int32(port, 80, "http server port");
DEF_string(key, "", "private key file");
DEF_string(ca, "", "certificate file");

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    FLG_cout = true;

    if (!FLG_key.empty() && !FLG_ca.empty()) {
        if (FLG_port == 80) FLG_port = 443;
    }

    // since co v3.0, no need to hold the http::Server object
    http::Server().on_req(
        [](const http::Req& req, http::Res& res) {
            if (req.is_method_get()) {
                if (req.url() == "/hello") {
                    res.set_status(200);
                    res.set_body("hello get");
                } else {
                    res.set_status(404);
                }
            } else if (req.is_method_post()) {
                if (req.url() == "/hello") {
                    res.set_status(200);
                    res.add_header("hello", "xxxxx");
                    res.set_body("hello post", 10);
                } else {
                    res.set_status(403);
                }
            } else if (req.is_method_put()) {
                if (req.url() == "/hello") {
                    res.set_status(200);
                    res.set_body("hello put");
                } else {
                    res.set_status(403);
                }
            } else if (req.is_method_delete()) {
                if (req.url() == "/hello") {
                    res.set_status(200);
                    res.set_body("hello delete");
                } else {
                    res.set_status(403);
                }
            } else {
                res.set_status(405); // method not allowed
            }
        }
    ).start(FLG_ip.c_str(), FLG_port, FLG_key.c_str(), FLG_ca.c_str());

    while (true) sleep::sec(1024);
    return 0;
}
