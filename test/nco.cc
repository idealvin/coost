#include "co/co.h"

DEF_uint32(n, 1000000, "coroutine number");

DEF_main(argc, argv) {
    for (int i = 0; i < FLG_n; ++i) {
        go([](){
            co::sleep(1000000);
        });
    }

    co::sleep(1000000);
    return 0;
}
