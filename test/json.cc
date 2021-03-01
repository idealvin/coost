#include "co/json.h"
#include "co/time.h"
#include "co/log.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();
    json::Value v;
    v.add_member("name", "vin");
    v.add_member("age", 23);
#if 0
    v.add_member("hello11111", "hello world");
    v.add_member("hello22222", "hello world");
    v.add_member("hello33333", "hello world");
    v.add_member("hello44444", "hello world");
    v.add_member("hello55555", "hello world");
#endif

    json::Value a;
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);

    json::Value b(a);

    v.add_member("num", a);

    json::Value o;
    o["o1"] = 3.14;
    o["o2"] = "good";
    o["o3"] = b;

    v.add_member("o", o);

    fastring s = v.str();
    COUT << s;

    json::Value u = json::parse(s.data(), s.size());
    if (!u.is_object()) {
        COUT << "parse error..";
        return -1;
    }

    COUT << u.str();
    COUT << u.pretty();

    if (u.has_member("name")) {
        COUT << "name: " << u["name"].get_string();
    }

    for (auto it = u.begin(); it != u.end(); ++it) {
        COUT << it->key << ": " << it->value.str();
    }

    json::Value ar = u["num"];
    COUT << "num:";
    for (uint32 i = 0; i < ar.size(); ++i) {
        COUT << ar[i].get_int() << ' ';
    }

    int n = 100000;

    int64 beg = now::us();
    for (int i = 0; i < n; ++i) {
        json::Value xx;
        xx.parse_from(s.data(), s.size());
    }
    int64 end = now::us();

    COUT << "parse average time used: " << (end - beg) * 1.0 / n << "us";

    json::Value xx = json::parse(s.data(), s.size());
    fastring xs;
    beg = now::us();
    for (int i = 0; i < n; ++i) {
        xs = xx.str();
    }
    end = now::us();

    COUT << "stringify average time used: " << (end - beg) * 1.0 / n << "us";

    beg = now::us();
    for (int i = 0; i < n; ++i) {
        xs = xx.pretty();
    }
    end = now::us();

    COUT << "pretty average time used: " << (end - beg) * 1.0 / n << "us";

    return 0;
}
