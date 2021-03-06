#include "co/all.h"

DEF_int32(n, 5, "");

void fun() {
    for (int i = 0; i < FLG_n; ++i) {
        COUT << "hello world";
        co::sleep(200);
    }
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    go(fun);
    sleep::ms(FLG_n * 200);

    return 0;
}
