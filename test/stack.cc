#include "co/log.h"
#include "co/time.h"
#include "co/co.h"
#include "co/thread.h"
#include <stdlib.h>

DEF_bool(t, false, "if true, run test in thread");
DEF_bool(m, false, "if true, run test in main thread");
DEF_bool(check, false, "if true, run CHECK test");
DEF_bool(assert, false, "if true, run runtime_assert test");
DEF_bool(abort, false, "if true, run abort test");

void a() {
    char* p = 0;
    if (FLG_check) {
        log::check_eq(1 + 1, 3);
    } else if (FLG_assert) {
        runtime_assert(1+1==3);
    } else if (FLG_abort) {
        ::abort();
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
    flag::parse(argc, argv, true);

    if (FLG_m) {
        c();
    } else if (FLG_t) {
        std::thread(c).detach();
    } else {
        go(c);
    }

    while (true) time::sleep(7000);

    return 0;
}
