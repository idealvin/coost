#include "co/co.h"
#include "co/cout.h"

DEF_uint32(n, 8, "coroutine number");

DEF_main(argc, argv) {
    co::wait_group wg;
    wg.add(FLG_n);

    for (uint32 i = 0; i < FLG_n; ++i) {
        go([wg]() {
            co::print("sched: ", co::sched_id(), " co: ", co::coroutine_id());
            wg.done();
        });
    }

    wg.wait();
    return 0;
}
