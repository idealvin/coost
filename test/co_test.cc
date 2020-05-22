#include "co/co.h"
#include "co/log.h"
#include "co/time.h"

co::Event ev;
co::Mutex mtx;
co::Pool pool;

int v = 0;
int n = 0;

void f1() {
    ev.wait();
    CLOG << "f1()";
    {
        co::MutexGuard g(mtx);
        ++v;
    }

    int* p = (int*) pool.pop();
    if (!p) p = new int(atomic_inc(&n));
    pool.push(p);
}

void f2() {
    bool r = ev.wait(50);
    CLOG << "f2() r: " << r;
}

void f3() {
    CLOG << "f3()";
    ev.signal();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    for (int i = 0; i < 8; ++i) go(f1);
    go(f2);

    sleep::ms(100);
    go(f3);

    sleep::ms(200);

    CLOG << "v: " << v;
    CLOG << "n: " << n;

    return 0;
}
