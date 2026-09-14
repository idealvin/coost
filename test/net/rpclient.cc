#include "co/co.h"
#include "co/rpc.h"
#include "co/flag.h"

// usage:
//   ./rpclient -data '{"api":"HelloWorld.world"}'
//   ./rpclient -ip 127.0.0.1 -port 7788 -data '{"api":"HelloWorld.world"}'

DEF_string(ip, "127.0.0.1", "server ip");
DEF_int32(port, 7788, "server port");
DEF_string(data, "{\"api\":\"ping\"}", "JSON body");

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    flag::set_value("also_log2console", "true");
    flag::set_value("rpc_log", "true");

    co::wait_group wg;
    wg.add();

    go([wg](){
        rpc::client c(FLG_ip.c_str(), FLG_port);
        json::any req = json::parse(FLG_data);
        json::any res;
        c.call(req, res);
        c.close();
        wg.done();
    });

    wg.wait();
    return 0;
}
