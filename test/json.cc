#include "co/json.h"
#include "co/time.h"
#include "co/log.h"

DEF_uint32(n, 64, "string length for this test");

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    json::Root v;
    v.add_member("name", "vin");
    v.add_member("age", 23);

    json::Value a = v.add_array("num");
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);

    json::Value o = v.add_object("o");
    o.add_member("o1", 3.14);
    o.add_member("o2", fastring(FLG_n, 'o'));
    json::Value o3 = o.add_array("o3");
    o3.push_back(1);
    o3.push_back(2);
    o3.push_back(3);

    fastring s = v.str();
    COUT << s;

    json::Root u = json::parse(s.data(), s.size());
    if (!u.is_object()) {
        COUT << "parse error..";
        return -1;
    }

    COUT << u.str();
    COUT << u.pretty();

    if (u.has_member("name")) {
        COUT << "name: " << u["name"].get_string();
    }

    int n = 100000;
    COUT << "s.size(): " << s.size();

    int64 beg = now::us();
    for (int i = 0; i < n; ++i) {
        json::Root xx = json::parse(s.data(), s.size());
    }
    int64 end = now::us();

    COUT << "parse average time used: " << (end - beg) * 1.0 / n << "us";

    json::Root xx = json::parse(s.data(), s.size());
    fastring xs;
    beg = now::us();
    for (int i = 0; i < n; ++i) {
        xs = xx.str(256 + FLG_n);
    }
    end = now::us();

    COUT << "stringify average time used: " << (end - beg) * 1.0 / n << "us";

    beg = now::us();
    for (int i = 0; i < n; ++i) {
        xs = xx.pretty(256 + FLG_n);
    }
    end = now::us();

    COUT << "pretty average time used: " << (end - beg) * 1.0 / n << "us";

    return 0;
}
