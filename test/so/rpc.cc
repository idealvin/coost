#include "rpc/hello_world.h"
#include "rpc/hello_again.h"
#include "co/co.h"
#include "co/time.h"

DEF_bool(c, false, "client or server");
DEF_int32(n, 1, "req num");
DEF_int32(conn, 1, "conn num");
DEF_string(serv_ip, "127.0.0.1", "server ip");
DEF_int32(serv_port, 7788, "server port");
DEF_bool(ping, false, "test rpc ping");
DEF_string(key, "", "private key file");
DEF_string(ca, "", "certificate file");
DEF_bool(ssl, false, "use ssl if true");

namespace xx {

class HelloWorldImpl : public HelloWorld {
  public:
    HelloWorldImpl() = default;
    virtual ~HelloWorldImpl() = default;

    virtual void hello(co::Json& req, co::Json& res) {
        res = {
            { "result", {
                { "hello", 23 }
            }}
        };
    }

    virtual void world(co::Json& req, co::Json& res) {
        res = {
            { "error", "not supported"}
        };
    }
};

class HelloAgainImpl : public HelloAgain {
  public:
    HelloAgainImpl() = default;
    virtual ~HelloAgainImpl() = default;

    virtual void hello(co::Json& req, co::Json& res) {
        res = {
            { "result", {
                { "hello", "again" }
            }}
        };
    }

    virtual void again(co::Json& req, co::Json& res) {
        res = {
            { "error", "not supported"}
        };
    }
};

} // xx

// proto client
std::unique_ptr<rpc::Client> proto;

// perform RPC request with rpc::Client
void test_rpc_client() {
    // copy a client from proto, 
    rpc::Client c(*proto);

    for (int i = 0; i < FLG_n; ++i) {
        co::Json req, res;
        req.add_member("api", "HelloWorld.hello");
        c.call(req, res);
    }

    for (int i = 0; i < FLG_n; ++i) {
        co::Json req, res;
        req.add_member("api", "HelloAgain.again");
    }

    c.close();
}

co::pool pool(
    []() { return (void*) new rpc::Client(*proto); },
    [](void* p) { delete (rpc::Client*) p; }
);

void test_ping() {
    co::pool_guard<rpc::Client> c(pool);
    while (true) {
        c->ping();
        co::sleep(3000);
    }
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    FLG_ssl = !FLG_key.empty() && !FLG_ca.empty();

    // initialize the proto client, other client can simply copy from it.
    proto.reset(new rpc::Client(FLG_serv_ip.c_str(), FLG_serv_port, FLG_ssl));

    if (!FLG_c) {
        // since co v3.0, no need to hold the rpc::Server object any more
        rpc::Server()
            .add_service(new xx::HelloWorldImpl)
            .add_service(new xx::HelloAgainImpl)
            .start("0.0.0.0", FLG_serv_port, "/hello", FLG_key.c_str(), FLG_ca.c_str());
    } else {
        if (FLG_ping) {
            go(test_ping);
            go(test_ping);
        } else {
            for (int i = 0; i < FLG_conn; ++i) {
                go(test_rpc_client);
            }
        }
    }

    while (true) sleep::sec(80000);
    return 0;
}
