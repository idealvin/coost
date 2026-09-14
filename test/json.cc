#include "co/json.h"
#include "co/print.h"
#include "co/flag.h"
#include "co/time.h"

DEF_uint32(n, 64, "string length for this test");

json::any f() {
    json::any v;
    v.add_member("name", "vin");
    v.add_member("age", 23);

    json::any a;
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);
    v.add_member("num", a);

    json::any o;
    o.add_member("o1", 3.14);
    o.add_member("o2", co::string(FLG_n, 'o'));
    json::any o3;
    o3.push_back(1);
    o3.push_back(2);
    o3.push_back(3);
    o.add_member("o3", o3);
    v.add_member("o", o);

    return v;
}

json::any g() {
    json::any v = {
        { "name", "vin" },
        { "age", 23 },
        { "num", {1, 2, 3} },
        { "o", {
            { "o1", 3.14 },
            { "o2", co::string(FLG_n, 'o') },
            { "o3", { 1, 2, 3 } }
        }}
    };
    return v;
}

json::any h() {
    return json::any()
        .add_member("name", "vin")
        .add_member("age", 23)
        .add_member("num", json::any().push_back(1).push_back(2).push_back(3))
        .add_member("o", json::any()
            .add_member("o1", 3.14)
            .add_member("o2", co::string(FLG_n, 'o'))
            .add_member("o3", json::any()
                .push_back(1).push_back(2).push_back(3)
            )
        );
}

int main(int argc, char** argv) {
    flag::parse(argc, argv, true);

    auto u = f();
    auto v = g();
    auto w = h();
    co::string s = u.str();
    co::println(s);
    co::println(v.str());
    co::println(w.str());

    u = json::parse(s.data(), s.size());
    if (!u.is_object()) {
        co::println("parse error..");
        return -1;
    }

    co::println(u.str());
    co::println(u.pretty());
    co::println("u[\"num\"][0] = ", u.get("num", 0));
    co::println("u[\"o\"][\"o3\"][1] = ", u.get("o", "o3", 1));

    int n = 10000;
    co::println("s.size(): ", s.size());

    int64 beg = time::mono.us();
    for (int i = 0; i < n; ++i) {
        json::any xx = json::parse(s.data(), s.size());
    }
    int64 end = time::mono.us();

    co::println("parse average time used: ", (end - beg) * 1.0 / n, "us");

    json::any xx = json::parse(s.data(), s.size());
    co::string xs;
    beg = time::mono.us();
    for (int i = 0; i < n; ++i) {
        xs = xx.str(256 + FLG_n);
    }
    end = time::mono.us();

    co::println("stringify average time used: ", (end - beg) * 1.0 / n, "us");

    beg = time::mono.us();
    for (int i = 0; i < n; ++i) {
        xs = xx.pretty(256 + FLG_n);
    }
    end = time::mono.us();

    co::println("pretty average time used: ", (end - beg) * 1.0 / n, "us");

    return 0;
}
