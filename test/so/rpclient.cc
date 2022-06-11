#include "co/co.h"
#include "co/rpc.h"

// usage:
//   ./rpclient -data '{"api":"HelloWorld.world"}'
//   ./rpclient -ip 127.0.0.1 -port 7788 -data '{"api":"HelloWorld.world"}'

DEF_string(ip, "127.0.0.1", "server ip");
DEF_int32(port, 7788, "server port");
DEF_bool(ssl, false, "use ssl if true");
DEF_string(data, "{\"api\":\"ping\"}", "JSON body");

co::WaitGroup wg;

void client_fun() {
    rpc::Client c(FLG_ip.c_str(), FLG_port, FLG_ssl);

    Json req = json::parse(FLG_data);
    Json res;
    c.call(req, res);

    c.close();
    wg.done();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    FLG_cout = true;

    wg.add();
    go(client_fun);
    wg.wait();
    return 0;
}
