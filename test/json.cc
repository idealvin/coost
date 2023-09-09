#include "co/json.h"
#include "co/cout.h"
#include "co/flag.h"
#include "co/time.h"
#include "co/defer.h"

DEF_uint32(n, 64, "string length for this test");

co::Json f() {
    co::Json v;
    v.add_member("name", "vin");
    v.add_member("age", 23);

    co::Json a;
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);
    v.add_member("num", a);

    co::Json o;
    o.add_member("o1", 3.14);
    o.add_member("o2", fastring(FLG_n, 'o'));
    co::Json o3;
    o3.push_back(1);
    o3.push_back(2);
    o3.push_back(3);
    o.add_member("o3", o3);
    v.add_member("o", o);

    return v;
}

co::Json g() {
    co::Json v = {
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

co::Json h() {
    return co::Json()
        .add_member("name", "vin")
        .add_member("age", 23)
        .add_member("num", co::Json().push_back(1).push_back(2).push_back(3))
        .add_member("o", co::Json()
            .add_member("o1", 3.14)
            .add_member("o2", fastring(FLG_n, 'o'))
            .add_member("o3", co::Json()
                .push_back(1).push_back(2).push_back(3)
            )
        );
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);

    auto u = f();
    auto v = g();
    auto w = h();
    fastring s = u.str();
    co::print(s);
    co::print(v.str());
    co::print(w.str());

    u = json::parse(s.data(), s.size());
    if (!u.is_object()) {
        co::print("parse error..");
        return -1;
    }

    co::print(u.str());
    co::print(u.pretty());
    co::print("u[\"num\"][0] = ", u.get("num", 0));
    co::print("u[\"o\"][\"o3\"][1] = ", u.get("o", "o3", 1));

    int n = 10000;
    co::print("s.size(): ", s.size());

    int64 beg = now::us();
    for (int i = 0; i < n; ++i) {
        co::Json xx = json::parse(s.data(), s.size());
    }
    int64 end = now::us();

    co::print("parse average time used: ", (end - beg) * 1.0 / n, "us");

    co::Json xx = json::parse(s.data(), s.size());
    fastring xs;
    beg = now::us();
    for (int i = 0; i < n; ++i) {
        xs = xx.str(256 + FLG_n);
    }
    end = now::us();

    co::print("stringify average time used: ", (end - beg) * 1.0 / n, "us");

    beg = now::us();
    for (int i = 0; i < n; ++i) {
        xs = xx.pretty(256 + FLG_n);
    }
    end = now::us();

    co::print("pretty average time used: ", (end - beg) * 1.0 / n, "us");

    return 0;
}
