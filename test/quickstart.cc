#include "co/all.h"

DEF_int32(n, 3, "times");
DEF_int32(ms, 500, "time in ms");

void fun() {
    for (int i = 0; i < FLG_n; ++i) {
        COUT << "hello world";
        co::sleep(FLG_ms);
    }
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    go(fun);
    sleep::ms(FLG_n * FLG_ms);

    return 0;
}
