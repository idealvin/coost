#include "co/all.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    Json v;
    v.push_back(0);
    v.push_back(v);

    COUT << v.size();
    COUT << v[0].get_int();
    COUT << v[1][0].get_int();
    COUT << v[1][1][0].get_int();
    COUT << v[1][1][1][0].get_int();

    Json o;
    o.add_member("name", "bob");
    o.add_member("o", o);

    COUT << o.size();
    COUT << o["name"].get_string();
    COUT << o["o"]["name"].get_string();
    COUT << o["o"]["o"]["name"].get_string();
    COUT << o["o"]["o"]["o"]["name"].get_string();

    return 0;
}
