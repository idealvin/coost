#include "co/flag.h"
#include "co/log.h"
#include "co/so.h"

DEF_string(d, ".", "root dir");
DEF_string(ip, "0.0.0.0", "http server ip");
DEF_int32(port, 80, "http server port");
DEF_string(key, "", "private key file");
DEF_string(ca, "", "certificate file");

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    if (!FLG_key.empty() && !FLG_ca.empty()) {
        if (FLG_port == 80) FLG_port = 443;
    }
    so::easy(FLG_d.c_str(), FLG_ip.c_str(), FLG_port, FLG_key.c_str(), FLG_ca.c_str()); // mum never have to worry again

    return 0;
}
