#include "co/co.h"
#include "co/rpc.h"

// usage:
//   ./rpclient -data '{"api":"HelloWorld.world"}'
//   ./rpclient -ip 127.0.0.1 -port 7788 -data '{"api":"HelloWorld.world"}'

DEF_string(ip, "127.0.0.1", "server ip");
DEF_int32(port, 7788, "server port");
DEF_bool(ssl, false, "use ssl if true");
DEF_string(data, "{\"api\":\"ping\"}", "JSON body");

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    FLG_cout = true;

    co::wait_group wg;
    wg.add();
    go([wg](){
        rpc::Client c(FLG_ip.c_str(), FLG_port, FLG_ssl);

        co::Json req = json::parse(FLG_data);
        co::Json res;
        c.call(req, res);

        c.close();
        wg.done();
    });
    wg.wait();
    return 0;
}
