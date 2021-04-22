#include "co/all.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    json::Root r;
    CLOG << r.is_null();

    r.add_member("name", "Bob");
    CLOG << "name: " << r["name"].get_string();

    r.add_member("age", 23);
    CLOG << "age: " << r["age"].get_int();

    r.add_member("male", true);
    CLOG << "male: " << r["male"].get_bool();

    r.add_member("height", 1.88);
    CLOG << "height: " << r["height"].get_double();

    json::Value v = r.add_array("num", 3);
    v.push_back(1, 2, 3);

    CLOG << "num: ";
    json::Value n = r["num"];
    for (auto it = n.begin(); it != n.end(); ++it) {
        CLOG << (*it).get_int();
    }

    for (auto it = r.begin(); it != r.end(); ++it) {
        CLOG << it.key() << ":" << it.value();
    }

    auto x = v.push_object();
    x.add_member("xxx", 3.14);
    
    auto y = x.add_array("arr");
    y.push_back(1, 2.3, "hello");

    CLOG << "size: " << r.size();
    CLOG << "array size: " << n.size();
    CLOG << "string size: " << r["name"].size();
    CLOG << "str:" << r.str();
    CLOG << "pretty: " << r.pretty();

    Json k = json::parse(r.str());
    k["name"] = "good";
    CLOG << k;

    k.set_object();
    k.set_array();
    k.set_null();
    CLOG << k;

    Json m(r);
    k = r;
    k["name"] = "kkkk";
    m["name"] = "mmmm";
    CLOG << "r>> " << r;
    CLOG << "k>> " << k;
    CLOG << "m>> " << m;

    return 0;
}
