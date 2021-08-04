#include "co/co.h"
#include "co/log.h"
#include "co/time.h"

DEF_main(argc, argv) {
    FLG_cout = true;

    co::WaitGroup wg;
    wg.add(8);

    for (int i = 0; i < 8; ++i) {
        go([wg]() {
            LOG << "co: " << co::coroutine_id();
            wg.done();
        });
    }

    wg.wait();
    LOG << "wg wait end..";

    return 0;
}
