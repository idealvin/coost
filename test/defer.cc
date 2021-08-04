#include "co/defer.h"
#include "co/log.h"
#include "co/time.h"

void f(int x, int y) {
    LOG << (x + y);
}

void f() {
    Timer t;
    defer(LOG << "time elapse: " << t.us() << " us");
    LOG << "hello f()";
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();
    FLG_cout = true;

    defer(LOG << "hello world");
    defer(LOG << "hello again");
    defer(f(1, 1); f(1, 3));
    f();
    f();
    return 0;
}
