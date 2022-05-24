#include "rpc/hello_world.h"
#include "rpc/hello_again.h"
#include "co/co.h"
#include "co/time.h"

DEF_bool(c, false, "client or server");
DEF_int32(n, 1, "req num");
DEF_int32(conn, 1, "conn num");
//DEF_string(userpass, "{\"bob\":\"nice\", \"alice\":\"nice\"}", "usernames and passwords for rpc server");
//DEF_string(username, "alice", "username for rpc client");
//DEF_string(password, "nice", "password for rpc client");
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
        res = {
            { "result", {
                { "hello", 23 }
            }}
        };
    }

    virtual void world(const Json& req, Json& res) {
        res = {
            { "error", "not supported"}
        };
    }
};

class HelloAgainImpl : public HelloAgain {
  public:
    HelloAgainImpl() = default;
    virtual ~HelloAgainImpl() = default;

    virtual void hello(const Json& req, Json& res) {
        res = {
            { "result", {
                { "hello", "again" }
            }}
        };
    }

    virtual void again(const Json& req, Json& res) {
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
    // and we needn't set username & password again.
    rpc::Client c(*proto);

    for (int i = 0; i < FLG_n; ++i) {
        Json req, res;
        req.add_member("api", "HelloWorld.hello");
        c.call(req, res);
    }

    for (int i = 0; i < FLG_n; ++i) {
        Json req, res;
        req.add_member("api", "HelloAgain.again");
    }

    c.close();
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
    FLG_cout = true;
    FLG_ssl = !FLG_key.empty() && !FLG_ca.empty();

    // initialize the proto client, other client can simply copy from it.
    proto.reset(new rpc::Client(FLG_serv_ip.c_str(), FLG_serv_port, FLG_ssl));
    //proto->set_userpass(FLG_username.c_str(), FLG_password.c_str());
    //FLG_password.safe_clear(); // clear the password

    rpc::Server serv;

    if (!FLG_c) {
        //serv.add_userpass(FLG_userpass.c_str());
        serv.add_service(new xx::HelloWorldImpl);
        serv.add_service(new xx::HelloAgainImpl);
        serv.start("0.0.0.0", FLG_serv_port, "/hello", FLG_key.c_str(), FLG_ca.c_str());
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

    while (true) {
        sleep::sec(80000);
    }

    return 0;
}
