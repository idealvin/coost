#include "co/all.h"

DEF_string(s, "https://github.com", "server url");
DEF_string(m, "", "method, GET, POST, DELETE, PUT");
DEF_string(url, "", "url of http request");
DEF_string(data, "{\"api\":\"ping\"}", "data to send");
DEF_string(path, "", "for PUT, path of the file to be uploaded");

co::wait_group wg;

void fa() {
    http::Client c(FLG_s.c_str());

    int r;
    LOG << "get /";
    c.get("/");
    r = c.status();
    LOG << "response code: " << r;
    LOG_IF(r == 0) << "error: " << c.strerror();
    LOG << "body size: " << c.body().size();
    LOG << "Content-Length: " << c.header("Content-Length");
    LOG << "Content-Type: " << c.header("Content-Type");
    LOG << c.header();

    LOG << "get /idealvin/co";
    c.get("/idealvin/co");
    r = c.status();
    LOG << "response code: " << r;
    LOG_IF(r == 0) << "error: " << c.strerror();
    LOG << "body size: " << c.body().size();
    LOG << "Content-Length: " << c.header("Content-Length");
    LOG << "Content-Type: " << c.header("Content-Type");
    LOG << c.header();

    // close the client before sending a signal
    c.close();
    wg.done();
}

void fb() {
    http::Client c(FLG_s.c_str());
    co::print(FLG_m, " ", FLG_url);
    if (FLG_m == "GET") {
        c.get(FLG_url.c_str());
    } else if (FLG_m == "POST") {
        c.post(FLG_url.c_str(), FLG_data.c_str());
    } else if (FLG_m == "PUT") {
        if (!FLG_path.empty()) {
            c.put(FLG_url.c_str(), FLG_path.c_str());
        }
    } else if (FLG_m == "DELETE") {
        c.del(FLG_url.c_str());
    } else {
        LOG << "method not supported: " << FLG_m;
    }

    LOG << c.header();
    LOG << c.body();
    c.close();
    wg.done();
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    FLG_cout = true;

    wg.add();
    if (FLG_m.empty() && FLG_url.empty()) {
        go(fa);
    } else {
        go(fb);
    }
    wg.wait();

    return 0;
}
