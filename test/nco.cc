#include "co/all.h"

DEF_uint32(n, 1000000, "coroutine number");
DEF_uint32(t, 1, "seconds to sleep in coroutines");

co::wait_group wg;

DEF_main(argc, argv) {
    COUT << "sleep " << FLG_t << " seconds\n";
    for (int i = 0; i < FLG_n; ++i) {
        wg.add();
        go([](){
            co::sleep(FLG_t * 1000);
            wg.done();
        });
    }

    wg.wait();
    return 0;
}
