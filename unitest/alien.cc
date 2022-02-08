#include "co/unitest.h"
#include <string.h>

#ifdef _WIN32
#include <intrin.h>
#endif

namespace test {

void parse_serv_url(const char* url, fastring& ip, int& port, bool& https);

inline uint32 find_index(uint32 size) {
  #ifdef _WIN32
    unsigned long index;
    _BitScanReverse(&index, size - 1);
    return index - 3;
  #else
    return 28 - __builtin_clz(size - 1);
  #endif
}

inline uint32 find_first_zero_bit(uint32 x) {
  #ifdef _WIN32
    unsigned long index;
    return _BitScanForward(&index, ~x) ? index : 32;
  #else
    const uint32 r = __builtin_ffs(~x);
    return r > 0 ? r - 1 : 32;
  #endif
}

DEF_test(alien) {
    DEF_case(find_index) {
        EXPECT_EQ(find_index(1u << 4), 0);
        EXPECT_EQ(find_index(1u << 5), 1);
        EXPECT_EQ(find_index(1u << 6), 2);
        EXPECT_EQ(find_index(1u << 7), 3);
        EXPECT_EQ(find_index(1u << 8), 4);
        EXPECT_EQ(find_index(1u << 9), 5);
        EXPECT_EQ(find_index(1u << 10), 6);
        EXPECT_EQ(find_index(1u << 11), 7);
        EXPECT_EQ(find_index(1u << 12), 8);

      #if 0
        for (uint32 x = 4; x < 12; ++x) {
            for (uint32 i = (1 << x) + 1; i < (1 << (x + 1)); ++i) {
                EXPECT_EQ(find_index(i), x - 3);
            }
        }
      #endif
    }

    DEF_case(find_first_zero_bit) {
        for (uint32 i = 1; i < 32; ++i) {
            EXPECT_EQ(find_first_zero_bit((1u << i) - 1), i);
            EXPECT_EQ(find_first_zero_bit((1u << i)), 0);
        }

        EXPECT_EQ(find_first_zero_bit(5), 1);
        EXPECT_EQ(find_first_zero_bit(23), 3);
        EXPECT_EQ(find_first_zero_bit(73), 1);
        EXPECT_EQ(find_first_zero_bit(107), 2);
        EXPECT_EQ(find_first_zero_bit((uint32)-1), 32);
    }

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
