#include "co/co.h"

DEF_main(argc, argv) {
    co::WaitGroup wg;
    wg.add();
    go([&](){
        COUT << "coroutine begin..";
        wg.done();
    });
    wg.wait();
    return 0;
}
