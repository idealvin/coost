#include "co/all.h"

DEF_string(ip, "127.0.0.1", "ip");
DEF_int32(port, 6688, "port");

void udp_server_fun() {
    sock_t fd = co::udp_socket();

    struct sockaddr_in addr;
    co::init_addr(&addr, FLG_ip.c_str(), FLG_port);
    co::bind(fd, &addr, sizeof(addr));

    struct sockaddr_in cli;
    int len = sizeof(cli);
    char buf[4];
    char pong[4]; memcpy(pong, "pong", 4);

    LOG << "server start";
    while (true) {
        len = sizeof(cli);
        int r = co::recvfrom(fd, buf, 4, &cli, &len);
        if (r >= 0) {
            LOG << "server recv " << fastring(buf, r) << " from "
                << co::addr2str(&cli);

            //r = co::sendto(fd, "pong", 4, &cli, len);
            r = co::sendto(fd, pong, 4, &cli, len);
            if (r == -1) {
                LOG << "server sendto error: " << co::strerror();
                break;
            } else {
                LOG << "server send pong ok";
            }
        } else {
            LOG << "server recvfrom error: " << co::strerror();
            break;
        }
    }

    co::close(fd);
}

void udp_client_fun() {
    sock_t fd = co::udp_socket();

    struct sockaddr_in addr;
    co::init_addr(&addr, FLG_ip.c_str(), FLG_port);

    char buf[4];
    char ping[4]; memcpy(ping, "ping", 4);

    while (true) {
        //int r = co::sendto(fd, "ping", 4, &addr, sizeof(addr));
        int r = co::sendto(fd, ping, 4, &addr, sizeof(addr));
        if (r == -1) {
            LOG << "client sendto error: " << co::strerror();
            break;
        } else {
            LOG << "client send ping ok";
            r = co::recvfrom(fd, buf, 4, NULL, NULL);
            if (r == -1) {
                LOG << "client recvform error: " << co::strerror();
                break;
            } else {
                LOG << "client recv " << fastring(buf, r) << '\n';
                co::sleep(3000);
            }
        }
    }

    co::close(fd);
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    FLG_cout = true;

    go(udp_server_fun);
    sleep::ms(32);
    go(udp_client_fun);

    while (true) sleep::sec(1000);

    return 0;
}
