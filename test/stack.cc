#include "co/log.h"
#include "co/thread.h"
#include "co/time.h"
#include "co/co.h"

DEF_bool(t, false, "if true, run test in thread");
DEF_bool(m, false, "if true, run test in main thread");
DEF_bool(check, false, "if true, run CHECK test");

void a() {
    char* p = 0;
    if (FLG_check) {
        CHECK_EQ(1 + 1, 3);
    } else {
        *p = 'c';
    }
}

void b() {
    a();
}

void c() {
    b();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);

    if (FLG_m) {
        c();
    } else if (FLG_t) {
        Thread(c).detach();
    } else {
        go(c);
    }

    while (1) sleep::sec(1024);

    return 0;
}
