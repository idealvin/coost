#include "co/co.h"
#include "co/so/rpc.h"

DEF_string(username, "", "username to logon");
DEF_string(password, "", "password of the user");
DEF_string(ip, "127.0.0.1", "server ip");
DEF_int32(port, 7788, "server port");
DEF_bool(ssl, false, "use ssl if true");

DEF_string(api, "", "api name");
DEF_string(json, "", "rpc request");

co::Event ev;

void client_fun() {
    rpc::Client c(FLG_ip.c_str(), FLG_port, FLG_ssl);
    c.set_userpass(FLG_username.c_str(), FLG_password.c_str());
    FLG_password.safe_clear(); // clear password in the memory

    Json req = json::parse(FLG_json);
    Json res;
    if (!FLG_api.empty()) req.add_member("api", FLG_api.c_str());
    c.call(req, res);

    c.close();
    ev.signal();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    FLG_cout = true;

    go(client_fun);
    ev.wait();
    return 0;
}