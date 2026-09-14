#include "co/co.h"
#include "co/print.h"
#include "co/flag.h"
#include "co/time.h"

co::coro_t* gco = 0;
co::wait_group wg;

void f() {
    co::println("coroutine starts: ", co::coroutine_id());
    gco = co::coroutine();
    co::println("yield coroutine: ", gco);
    co::yield();
    co::println("coroutine ends: ", co::coroutine_id());
    wg.done();
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);

    wg.add(1);
    go(f);
    time::sleep(1000);
    if (gco) {
        co::println("resume coroutine: ", gco);
        co::resume(gco);
    }

    wg.wait();
    return 0;
}
