#include "co/co.h"
#include "co/cout.h"
#include "co/time.h"

void* gco = 0;
co::wait_group wg;

void f() {
    co::print("coroutine starts: ", co::coroutine_id());
    gco = co::coroutine();
    co::print("yield coroutine: ", gco);
    co::yield();
    co::print("coroutine ends: ", co::coroutine_id());
    wg.done();
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);

    wg.add(1);
    go(f);
    sleep::ms(10);
    if (gco) {
        co::print("resume coroutine: ", gco);
        co::resume(gco);
    }

    wg.wait();
    return 0;
}
