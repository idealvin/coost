#include "co/co.h"

DEF_main(argc, argv) {
    FLG_cout = true;

    LOG << "channel without timeout..";
    co::Chan<int> ch;
    go([ch]() {
        LOG << "ch << 7";
        ch << 7;
    });

    int v = 0;
    ch >> v;
    LOG << "v: " << v;

    // channel with a timeout
    LOG << "channel with timeout..";
    co::Chan<int> ch2(8/*cap*/, 100/*timeout in ms*/);

    for (int i = 0; i < 8; ++i) {
        go([ch2, i]() {
            if (i == 1) co::sleep(200);
            LOG << "ch2 << " << i;
            ch2 << i; // write to channel
        });
    }

    for (int i = 0; i < 8; ++i) {
        int v = 0;
        ch2 >> v; // read from channel
        if (!co::timeout()) {
            LOG << "v: " << v;
        } else {
            LOG << "timeout..";
        }
    }

    return 0;
}
