#include "base/co.h"
#include "base/log.h"
#include "base/time.h"

co::Event ev;
co::Mutex mtx;
co::Pool pool;
//co::Pool pool([] { return new int; }, [](void* p) { delete (int*) p; });

int v = 0;
int n = 0;

void co_f1() {
    ev.wait();
    CLOG << "co_f1()";
    {
        co::MutexGuard g(mtx);
        ++v;
    }

    int* p = (int*) pool.pop();
    if (!p) p = new int(atomic_inc(&n));
    pool.push(p);
}

void co_f2() {
    bool r = ev.wait(50);
    CLOG << "co_f2() r: " << r;
}

void co_f3() {
    CLOG << "co_f3()";
    ev.signal();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    for (int i = 0; i < 8; ++i) go(co_f1);
    go(co_f2);

    sleep::ms(100);
    go(co_f3);

    sleep::ms(200);

    CLOG << "v: " << v;
    CLOG << "n: " << n;

    return 0;
}
