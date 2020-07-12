#include "co/flag.h"
#include "co/log.h"
#include "co/so.h"

DEF_string(root, ".", "root dir");
DEF_string(ip, "0.0.0.0", "http server ip");
DEF_int32(port, 80, "http server port");

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    so::easy(FLG_root.c_str(), FLG_ip.c_str(), FLG_port); // mum never have to worry again

    return 0;
}
