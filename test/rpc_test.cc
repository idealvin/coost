#include "rpc/hello_world.h"
#include "base/flag.h"
#include "base/log.h"
#include "base/co/co.h"
#include "base/time.h"

DEF_bool(c, 1, "client or server");
DEF_int32(n, 1, "req num");
DEF_int32(conn, 1, "conn num");
DEF_string(user, "", "username");
DEF_string(passwd, "", "passwd");
DEF_string(serv_ip, "127.0.0.1", "server ip");

namespace xx {

void HelloWorld::hello(const Json& req, Json& res) {
    res.add_member("method", "hello");
    res.add_member("err", 200);
    res.add_member("errmsg", "200 ok");
}

void HelloWorld::world(const Json& req, Json& res) {
    res.add_member("method", "world");
    res.add_member("err", 200);
    res.add_member("errmsg", "200 ok");
}

} // xx

void client_fun() {
    rpc::Client* c = rpc::new_client(FLG_serv_ip.c_str(), 7788, FLG_passwd.c_str());

    for (int i = 0; i < FLG_n; ++i) {
        Json req, res;
        req.add_member("method", "hello");
        c->call(req, res);
    }

    delete c;
}

int main(int argc, char** argv) {
    flag::init(argc, argv);

    if (!FLG_c) {
        rpc::Server* server = rpc::new_server("", 7788, FLG_passwd.c_str()); 
        server->add_service(new xx::HelloWorld);
        server->start();

    } else {
        for (int i = 0; i < FLG_conn; ++i) {
            go(&client_fun);
        }
    }

    while (true) {
        sleep::sec(80000);
    }

    return 0;
}
