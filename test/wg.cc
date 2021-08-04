#include "co/co.h"
#include "co/log.h"
#include "co/time.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();
    FLG_cout = true;

    co::WaitGroup wg;

    for (int i = 0; i < 8; ++i) {
        wg.add();
        go([&]() {
            LOG << "co: " << co::coroutine_id();
            wg.done();
        });
    }

    wg.wait();
    LOG << "wg wait end..";

    return 0;
}
