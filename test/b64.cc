#include "co/all.h"
#include "co/base64.h"

DEF_bool(d, false, "decode");

int main(int argc, char** argv) {
    auto v = flag::parse(argc, argv);
    if (v.size() != 1) {
        co::print("usage: \n\tb64 xx.txt\n\tb64 -d xx.txt\n");
        return 0;
    }

    fs::file f(v[0], 'r');
    if (!f) {
        co::print("cannot open file: ", v[0], '\n');
        return 0;
    }

    auto s = f.read(f.size());
    if (FLG_d) {
        s = co::base64_decode(s);
    } else {
        s = co::base64_encode(s);
    }

    f.close();

    if (!f.open(v[0], 'w')) {
        co::print("cannot open file: ", v[0], '\n');
        return 0;
    }

    auto n = f.write(s);
    if (n != s.size()) {
        co::print("write file failed\n");
    }

    f.close();
    return 0;
}