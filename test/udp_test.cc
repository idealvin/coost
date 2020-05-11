#include "base/base.h"

DEF_string(ip, "127.0.0.1", "ip");
DEF_int32(port, 6688, "port");

void udp_server_fun() {
    sock_t fd = co::udp_socket();

    struct sockaddr_in addr;
    co::init_ip_addr(&addr, FLG_ip.c_str(), FLG_port);
    co::bind(fd, &addr, sizeof(addr));

    struct sockaddr_in cli;
    int len = sizeof(cli);
    char* buf = (char*) malloc(65536);

    while (true) {
        len = sizeof(cli);
        int r = co::recvfrom(fd, buf, 65507, &cli, &len);
        if (r >= 0) {
            COUT << "server recv " << fastring(buf, r) << " from "
                 << co::ip_str(&cli) << ':' << ntoh16(cli.sin_port);

            r = co::sendto(fd, "pong", 4, &cli, len);
            if (r == -1) {
                COUT << "server sendto error: " << co::strerror();
                break;
            } else {
                COUT << "server send pong ok";
            }
        } else {
            COUT << "server recvfrom error: " << co::strerror();
            break;
        }
    }

    free(buf);
    co::close(fd);
}

void udp_client_fun() {
    sock_t fd = co::udp_socket();

    struct sockaddr_in addr;
    co::init_ip_addr(&addr, FLG_ip.c_str(), FLG_port);

    char* buf = (char*) malloc(65536);

    while (true) {
        int r = co::sendto(fd, "ping", 4, &addr, sizeof(addr));
        if (r == -1) {
            COUT << "client sendto error: " << co::strerror();
            break;
        } else {
            COUT << "client send ping ok";
            r = co::recvfrom(fd, buf, 65537, NULL, NULL);
            if (r == -1) {
                COUT << "client recvform error: " << co::strerror();
                break;
            } else {
                COUT << "client recv " << fastring(buf, r) << '\n';
                co::sleep(3000);
            }
        }
    }

    free(buf);
    co::close(fd);
}

int main(int argc, char** argv) {
    flag::init(argc, argv);

    go(udp_server_fun);
    sleep::ms(32);
    go(udp_client_fun);

    while (true) sleep::sec(1000);

    return 0;
}
