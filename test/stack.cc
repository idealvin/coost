#include "co/log.h"
#include "co/time.h"
#include "co/co.h"

DEF_bool(t, false, "if true, run test in thread");
DEF_bool(m, false, "if true, run test in main thread");
DEF_bool(check, false, "if true, run CHECK test");

co::WaitGroup wg;

void a() {
    char* p = 0;
    if (FLG_check) {
        CHECK_EQ(1 + 1, 3);
    } else {
        *p = 'c';
    }
    wg.done();
}

void b() {
    a();
}

void c() {
    b();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);

    wg.add();
    if (FLG_m) {
        c();
    } else if (FLG_t) {
        std::thread(c).detach();
    } else {
        go(c);
    }

    wg.wait();
    return 0;
}
