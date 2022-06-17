#include "co/json.h"
#include "co/cout.h"
#include "co/flag.h"
#include "co/time.h"
#include "co/defer.h"

DEF_uint32(n, 64, "string length for this test");

Json f() {
    Json v;
    v.add_member("name", "vin");
    v.add_member("age", 23);

    Json a;
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);
    v.add_member("num", a);

    Json o;
    o.add_member("o1", 3.14);
    o.add_member("o2", fastring(FLG_n, 'o'));
    Json o3;
    o3.push_back(1);
    o3.push_back(2);
    o3.push_back(3);
    o.add_member("o3", o3);
    v.add_member("o", o);

    return v;
}

Json g() {
    Json v = {
        { "name", "vin" },
        { "age", 23 },
        { "num", {1, 2, 3} },
        { "o", {
            { "o1", 3.14 },
            { "o2", fastring(FLG_n, 'o') },
            { "o3", { 1, 2, 3 } }
        }}
    };
    return v;
}

Json h() {
    return Json()
        .add_member("name", "vin")
        .add_member("age", 23)
        .add_member("num", Json().push_back(1).push_back(2).push_back(3))
        .add_member("o", Json()
            .add_member("o1", 3.14)
            .add_member("o2", fastring(FLG_n, 'o'))
            .add_member("o3", Json()
                .push_back(1).push_back(2).push_back(3)
            )
        );
}

int main(int argc, char** argv) {
    flag::init(argc, argv);

    auto u = f();
    auto v = g();
    auto w = h();
    fastring s = u.str();
    COUT << s;
    COUT << v.str();
    COUT << w.str();

    u = json::parse(s.data(), s.size());
    if (!u.is_object()) {
        COUT << "parse error..";
        return -1;
    }

    COUT << u.str();
    COUT << u.pretty();
    COUT << "u[\"num\"][0] = " << u.get("num", 0);
    COUT << "u[\"o\"][\"o3\"][1] = " << u.get("o", "o3", 1);

    int n = 10000;
    COUT << "s.size(): " << s.size();

    int64 beg = now::us();
    for (int i = 0; i < n; ++i) {
        Json xx = json::parse(s.data(), s.size());
    }
    int64 end = now::us();

    COUT << "parse average time used: " << (end - beg) * 1.0 / n << "us";

    Json xx = json::parse(s.data(), s.size());
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
