#include "co/all.h"

DEF_string(ip, "127.0.0.1", "ip");
DEF_int32(port, 9988, "port");
DEF_string(key, "", "private key file");
DEF_string(ca, "", "certificate file");
DEF_int32(t, 0, "0: server & client, 1: server, 2: client");

struct Header {
    int32 magic;
    int32 body_len;
};

void on_connection(tcp::Connection conn) {
    const char* msg = "hello client";
    Header header;
    fastream buf(128);
    int32 body_len;
    int r;

    while (true) {
        r = conn.recvn(&header, sizeof(header), 3000);
        if (r != sizeof(header)) {
            ELOG << "server recvn error: " << conn.strerror();
            goto err;
        }

        body_len = ntoh32(header.body_len);
        LOG << "server recv header, body_len: " << body_len;

        buf.clear();
        r = conn.recvn((char*)buf.data(), body_len, 3000);
        if (r != body_len) {
            ELOG << "server recvn error: " << conn.strerror();
            goto err;
        }

        buf.resize(body_len);
        LOG << "server recv: " << buf;

        header.magic = hton32(777);
        header.body_len = hton32((uint32)strlen(msg));
        buf.clear();
        buf.append(&header, sizeof(header));
        buf.append(msg);

        r = conn.send(buf.data(), (int)buf.size(), 3000);
        if (r <= 0) {
            ELOG << "server send error: " << conn.strerror();
            goto err;
        }
    }

  err:
    conn.close();
}

void client_fun() {
    tcp::Client c(FLG_ip.c_str(), FLG_port, true);
    if (!c.connect(3000)) {
        LOG << "connect failed: " << c.strerror();
        return;
    }

    const char* msg = "hello server";
    Header header;
    fastream buf(128);
    int32 body_len;
    int r;

    while (true) {
        header.magic = hton32(777);
        header.body_len = hton32((uint32)strlen(msg));
        buf.clear();
        buf.append(&header, sizeof(header));
        buf.append(msg);

        r = c.send(buf.data(), (int)buf.size(), 3000);
        if (r <= 0) {
            ELOG << "client send error: " << c.strerror();
            goto err;
        }

        r = c.recvn(&header, sizeof(header), 3000);
        if (r != sizeof(header)) {
            ELOG << "client recvn error: " << c.strerror();
            goto err;
        }

        body_len = ntoh32(header.body_len);
        LOG << "client recv header, body_len: " << body_len;

        buf.clear();
        r = c.recvn((char*)buf.data(), body_len, 3000);
        if (r != body_len) {
            ELOG << "ssl client recvn error: " << c.strerror();
            goto err;
        }

        buf.resize(body_len);
        LOG << "client recv: " << buf << '\n';

        co::sleep(2000);
    }

  err:
    c.disconnect();
    return;
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    FLG_cout = true;

    tcp::Server serv;
    serv.on_connection(on_connection);
    CHECK(!FLG_key.empty()) << "ssl private key file not set..";
    CHECK(!FLG_ca.empty()) << "ssl certificate file not set..";

    if (FLG_t == 0) {
        serv.start(FLG_ip.c_str(), FLG_port, FLG_key.c_str(), FLG_ca.c_str());
        sleep::ms(32);
        go(client_fun);
    } else if (FLG_t == 1) {
        serv.start(FLG_ip.c_str(), FLG_port, FLG_key.c_str(), FLG_ca.c_str());
    } else {
        go(client_fun);
    }

    while (true) sleep::sec(1024);
}
