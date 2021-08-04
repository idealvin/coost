#include "co/co.h"

DEF_main(argc, argv) {
    FLG_cout = true;
    co::WaitGroup wg;

    for (int i = 0; i < 16; ++i) {
        wg.add();
        go([wg, i](){
            LOG << "coroutine begin: " << i;
            wg.done();
            LOG << "coroutine done: " << i;
        });
    }

    wg.wait();
    return 0;
}
