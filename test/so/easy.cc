#include "co/all.h"

DEF_string(d, ".", "root dir");
DEF_string(ip, "0.0.0.0", "http server ip");
DEF_int32(port, 8080, "http server port");

int main(int argc, char** argv) {
    flag::init(argc, argv);
    go([]() {
      co::sleep(1000);
      http::Client c("https://github.com");
      c.get("/");
      int http_status = c.status();
      CHECK_EQ(http_status, 200);
      COUT << "http client staus: " << http_status << " OK";
      exit(0);
    });

    COUT << "http server on http://" << FLG_ip << ":" << FLG_port;
    so::easy(FLG_d.c_str(), FLG_ip.c_str(), FLG_port); // mum never have to worry again
    return 0;
}
