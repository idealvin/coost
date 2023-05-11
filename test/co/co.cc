#include "co/co.h"
#include "co/log.h"
#include "co/time.h"
#include "co/cout.h"

DEF_bool(m, false, "main thread as scheduler");

co::event ev;
co::mutex mtx;
co::pool pool;

int v = 0;
int n = 0;

void f1() {
    ev.wait();
    co::print("f1()");

    {
        co::mutex_guard g(mtx);
        ++v;
    }

    int* p = (int*) pool.pop();
    if (!p) p = new int(atomic_inc(&n));
    pool.push(p);
}

void f() {
    co::sleep(32);
    co::print("s: ", co::sched_id(), " c: ", co::coroutine_id());
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);

    co::MainSched* ms = 0;
    if (FLG_m) ms = co::main_sched();

    for (int i = 0; i < 8; ++i) go(f1);
    go([]() {
        bool r = ev.wait(50);
        co::print("f2() r: ", r);
    });

    sleep::ms(100);
    go([]() {
        co::print("f3()");
        ev.signal();
    });
    
    co::print("co::event wait in non-coroutine beg: ", now::ms());
    ev.wait(200);
    co::print("co::event wait in non-coroutine end: ", now::ms());
    sleep::ms(200);

    co::print("v: ", v);
    co::print("n: ", n);

    for (int i = 0; i < 32; ++i) go(f);

    if (FLG_m) ms->loop();
    sleep::ms(100);
    return 0;
}
