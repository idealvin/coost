#include "co/all.h"

#ifdef HAS_LIBCURL
DEF_string(s, "https://github.com", "server url");
DEF_string(m, "", "method, GET, POST, DELETE, PUT");
DEF_string(url, "", "url of http request");

SyncEvent ev;

void fa() {
    http::Client c(FLG_s.c_str());
    //http headers can be added here before the request was performed.
    //c.add_header("hello", "world");

    int r;
    LOG << "get /";
    c.get("/");
    r = c.response_code();
    LOG << "response code: " << r;
    LOG_IF(r == 0) << "error: " << c.strerror();
    LOG << "body size: " << c.body_size();
    LOG << "Content-Length: " << c.header("Content-Length");
    LOG << c.header();

    LOG << "get /idealvin/co";
    c.get("/idealvin/co");
    r = c.response_code();
    LOG << "response code: " << r;
    LOG_IF(r == 0) << "error: " << c.strerror();
    LOG << "body size: " << c.body_size();
    LOG << "Content-Length: " << c.header("Content-Length");
    LOG << c.header();

    // close the client before sending a signal
    c.close();
    ev.signal();
}

void fb() {
    http::Client c(FLG_s.c_str());
    COUT << FLG_m << " " << FLG_url;
    if (FLG_m == "GET") {
        c.get(FLG_url.c_str());
    } else if (FLG_m == "POST") {
        c.post(FLG_url.c_str(), "hello world");
    } else if (FLG_m == "PUT") {
        c.put(FLG_url.c_str(), "hello world");
    } else if (FLG_m == "DELETE") {
        c.del(FLG_url.c_str());
    } else {
        LOG << "method not supported: " << FLG_m;
    }

    LOG << c.header();
    LOG << fastring(c.body(), c.body_size());

    c.close();
    ev.signal();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    FLG_cout = true;
    log::init();

    if (FLG_m.empty() && FLG_url.empty()) {
        go(fa);
    } else {
        go(fb);
    }

    ev.wait();
    return 0;
}
#else
int main(int argc, char** argv) {
    COUT << "libcurl required..";
    return 0;
}
#endif
