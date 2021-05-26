#include "rpc/hello_world.h"
#include "rpc/hello_again.h"
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
DEF_int32(serv_port, 7788, "server port");
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

// proto client
std::unique_ptr<rpc::Client> proto;

// perform RPC request with rpc::Client
void test_rpc_client() {
    // copy a client from proto, 
    // and we needn't set username & password again.
    rpc::Client c(*proto);

    for (int i = 0; i < FLG_n; ++i) {
        Json req, res;
        req.add_member("service", "xx.HelloWorld");
        req.add_member("method", "world");
        c.call(req, res);
    }

    for (int i = 0; i < FLG_n; ++i) {
        Json req, res;
        req.add_member("service", "xx.HelloAgain");
        req.add_member("method", "again");
        c.call(req, res);
    }

    c.close();
}

// perform RPC request with HelloWorldClient
std::unique_ptr<xx::HelloWorldClient> hello_world_proto;

void test_client() {
    xx::HelloWorldClient c(*hello_world_proto);
    Json req = c.make_req_hello();
    req.add_member("xxx", 123456);
    Json res = c.perform(req);

    req = c.make_req_world();
    req.add_member("xxx", 123456);
    res = c.perform(req);
}

co::Pool pool(
    []() { return (void*) new rpc::Client(*proto); },
    [](void* p) { delete (rpc::Client*) p; }
);

void test_ping() {
    co::PoolGuard<rpc::Client> c(pool);

    while (true) {
        c->ping();
        co::sleep(FLG_hb);
    }
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();
    FLG_cout = true;

    // initialize the proto client, other client can simply copy from it.
    proto.reset(new rpc::Client(FLG_serv_ip.c_str(), FLG_serv_port, FLG_ssl));
    proto->set_userpass(FLG_username.c_str(), FLG_password.c_str());

    hello_world_proto.reset(new xx::HelloWorldClient(FLG_serv_ip.c_str(), FLG_serv_port, FLG_ssl));
    hello_world_proto->set_userpass(FLG_username.c_str(), FLG_password.c_str());
    FLG_password.safe_clear(); // clear the password

    rpc::Server serv;

    if (!FLG_c) {
        serv.add_userpass(FLG_userpass.c_str());
        serv.add_service(new xx::HelloWorldImpl);
        serv.add_service(new xx::HelloAgainImpl);
        serv.start("0.0.0.0", FLG_serv_port, FLG_key.c_str(), FLG_ca.c_str());
    } else {
        if (FLG_ping) {
            go(test_ping);
            go(test_ping);
        } else {
            for (int i = 0; i < FLG_conn; ++i) {
                go(test_rpc_client);
            }
            go(test_client);
        }
    }

    while (true) {
        sleep::sec(80000);
    }

    return 0;
}
