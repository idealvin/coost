#include "hello_world.h"
#include "hello_again.h"
#include "co/flag.h"
#include "co/log.h"
#include "co/co.h"
#include "co/time.h"

DEF_bool(c, false, "client or server");
DEF_int32(n, 1, "req num");
DEF_int32(conn, 1, "conn num");
DEF_string(userpass, "{\"bob\":\"nice\", \"alice\":\"nice\"}", "usernames and passwords for rpc server");
DEF_string(username, "alice", "username for rpc client");
DEF_string(password, "nice", "password for rpc client");
DEF_string(serv_ip, "127.0.0.1", "server ip");
DEF_bool(ping, false, "test rpc ping");
DEF_int32(hb, 3000, "heartbeat");
DEF_string(key, "", "private key file");
DEF_string(ca, "", "certificate file");
DEF_bool(ssl, false, "use ssl if true");

namespace xx {

class HelloWorldImpl : public HelloWorld {
  public:
    HelloWorldImpl() = default;
    virtual ~HelloWorldImpl() = default;

    virtual void hello(const Json& req, Json& res) {
        res.add_member("method", "hello");
        res.add_member("err", 200);
        res.add_member("errmsg", "ok");
    }

    virtual void world(const Json& req, Json& res) {
        res.add_member("method", "world");
        res.add_member("err", 200);
        res.add_member("errmsg", "ok");
    }
};

class HelloAgainImpl : public HelloAgain {
  public:
    HelloAgainImpl() = default;
    virtual ~HelloAgainImpl() = default;

    virtual void hello(const Json& req, Json& res) {
        res.add_member("method", "hello");
        res.add_member("err", 200);
        res.add_member("errmsg", "ok");
    }

    virtual void again(const Json& req, Json& res) {
        res.add_member("method", "again");
        res.add_member("err", 200);
        res.add_member("errmsg", "ok");
    }
};

} // xx

void client_fun() {
    bool use_ssl = FLG_ssl || (!FLG_key.empty() && !FLG_ca.empty());
    rpc::Client* c = new rpc::Client(FLG_serv_ip.c_str(), 7788, use_ssl);
    c->set_userpass(FLG_username.c_str(), FLG_password.c_str());

    for (int i = 0; i < FLG_n; ++i) {
        Json req, res;
        req.add_member("service", "xx.HelloWorld");
        req.add_member("method", "world");
        c->call(req, res);
    }

    for (int i = 0; i < FLG_n; ++i) {
        Json req, res;
        req.add_member("service", "xx.HelloAgain");
        req.add_member("method", "again");
        c->call(req, res);
    }

    delete c;
}

void test_ping() {
    bool use_ssl = FLG_ssl || (!FLG_key.empty() && !FLG_ca.empty());
    rpc::Client* c = new rpc::Client(FLG_serv_ip.c_str(), 7788, use_ssl);
    c->set_userpass(FLG_username.c_str(), FLG_password.c_str());

    while (true) {
        c->ping();
        co::sleep(FLG_hb);
    }
    delete c;
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();
    FLG_cout = true;

    rpc::Server serv;

    if (!FLG_c) {
        serv.add_userpass(FLG_userpass.c_str());
        serv.add_service(new xx::HelloWorldImpl);
        serv.add_service(new xx::HelloAgainImpl);
        serv.start("0.0.0.0", 7788, FLG_key.c_str(), FLG_ca.c_str());
    } else {
        if (FLG_ping) {
            go(&test_ping);
        } else {
            for (int i = 0; i < FLG_conn; ++i) {
                go(&client_fun);
            }
        }
    }

    while (true) {
        sleep::sec(80000);
    }

    return 0;
}
