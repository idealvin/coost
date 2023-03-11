#include "co/co.h"
#include "co/cout.h"
#include "co/time.h"

void* gco = 0;
co::WaitGroup wg;

void f() {
    co::print("coroutine starts: ", co::coroutine_id());
    co::print("yield coroutine..");
    gco = co::coroutine();
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
        co::print("resume co: ", gco);
        co::resume(gco);
    }

    wg.wait();
    return 0;
}
