#include "co/co.h"
#include "co/cout.h"
#include "co/time.h"

void* gco = 0;
co::WaitGroup wg;

void f() {
    COUT << "coroutine starts: " << co::coroutine_id();
    COUT << "yield coroutine..";
    gco = co::coroutine();
    co::yield();
    COUT << "coroutine ends: " << co::coroutine_id();
    wg.done();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);

    wg.add(1);
    go(f);
    sleep::ms(10);
    if (gco) {
        COUT << "resume co: " << gco;
        co::resume(gco);
    }

    wg.wait();
    return 0;
}
