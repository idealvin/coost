#include "co/flag.h"
#include "co/http.h"

DEF_string(d, ".", "root dir");
DEF_string(ip, "0.0.0.0", "http server ip");
DEF_int32(port, 80, "http server port");

int main(int argc, char** argv) {
    flag::init(argc, argv);
    so::easy(FLG_d.c_str(), FLG_ip.c_str(), FLG_port); // mum never have to worry again
    return 0;
}
