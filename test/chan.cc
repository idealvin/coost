#include "co/co.h"
#include "co/cout.h"

void f() {
    co::Chan<int> ch;
    go([ch]() { ch << 7; });
    int v = 0;
    ch >> v;
    LOG << "v: " << v;
}

void g() {
    co::Chan<int> ch(32, 500);
    go([ch]() {
        ch << 7;
        if (co::timeout()) LOG << "write to channel timeout..";
    });

    int v = 0;
    ch >> v;
    if (!co::timeout()) LOG << "v: " << v;
}

DEF_main(argc, argv) {
    FLG_cout = true;
    f();
    g();
    return 0;
}
