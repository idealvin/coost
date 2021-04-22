#include "co/unitest.h"
#include <string.h>

namespace test {

void parse_serv_url(const char* url, fastring& ip, int& port, bool& https);

DEF_test(alien) {
    DEF_case(parse_serv_url) {
        fastring ip;
        int port;
        bool https;

        parse_serv_url("127.0.0.1", ip, port, https);
        EXPECT_EQ(ip, "127.0.0.1");
        EXPECT_EQ(port, 80);

        parse_serv_url("127.0.0.1:8080", ip, port, https);
        EXPECT_EQ(ip, "127.0.0.1");
        EXPECT_EQ(port, 8080);
        EXPECT_EQ(https, false);

        parse_serv_url("http://127.0.0.1:7777", ip, port, https);
        EXPECT_EQ(ip, "127.0.0.1");
        EXPECT_EQ(port, 7777);
        EXPECT_EQ(https, false);

        parse_serv_url("https://127.0.0.1", ip, port, https);
        EXPECT_EQ(ip, "127.0.0.1");
        EXPECT_EQ(port, 443);
        EXPECT_EQ(https, true);

        parse_serv_url("http://github.com", ip, port, https);
        EXPECT_EQ(ip, "github.com");
        EXPECT_EQ(port, 80);
        EXPECT_EQ(https, false);

        parse_serv_url("https://github.com", ip, port, https);
        EXPECT_EQ(ip, "github.com");
        EXPECT_EQ(port, 443);
        EXPECT_EQ(https, true);

        parse_serv_url("https://github.com:8888", ip, port, https);
        EXPECT_EQ(ip, "github.com");
        EXPECT_EQ(port, 8888);
        EXPECT_EQ(https, true);

        parse_serv_url("::1", ip, port, https);
        EXPECT_EQ(ip, "::1");
        EXPECT_EQ(port, 80);
        EXPECT_EQ(https, false);

        parse_serv_url("[::1]:8888", ip, port, https);
        EXPECT_EQ(ip, "::1");
        EXPECT_EQ(port, 8888);
        EXPECT_EQ(https, false);

        parse_serv_url("http://::1", ip, port, https);
        EXPECT_EQ(ip, "::1");
        EXPECT_EQ(port, 80);
        EXPECT_EQ(https, false);

        parse_serv_url("https://::1", ip, port, https);
        EXPECT_EQ(ip, "::1");
        EXPECT_EQ(port, 443);
        EXPECT_EQ(https, true);

        parse_serv_url("http://[::1]:7777", ip, port, https);
        EXPECT_EQ(ip, "::1");
        EXPECT_EQ(port, 7777);
        EXPECT_EQ(https, false);

        parse_serv_url("https://[::1]:7777", ip, port, https);
        EXPECT_EQ(ip, "::1");
        EXPECT_EQ(port, 7777);
        EXPECT_EQ(https, true);
    }
}

void parse_serv_url(const char* url, fastring& ip, int& port, bool& https) {
    const char* p = url;
    ip.clear();
    port = 0;
    https = false;

    if (memcmp(url, "https://", 8) == 0) {
        p += 8;
        https = true;
    } else {
        if (memcmp(url, "http://", 7) == 0) p += 7;
    }

    const char* s = strchr(p, ':');
    const char* r;
    if (s != NULL) {
        r = strrchr(p, ':');
        if (r == s) {                 /* ipv4:port */
            ip = fastring(p, r - p);
            port = atoi(r + 1);
        } else {                      /* ipv6 */
            if (*(r - 1) == ']') {    /* ipv6:port */
                if (*p == '[') ++p;
                ip = fastring(p, r - p - 1);
                port = atoi(r + 1);
            }
        }
    }

    if (ip.empty()) ip = p;
    if (port == 0) port = https ? 443 : 80;
}

} // namespace test
