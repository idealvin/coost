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
    LOG << "f1()";
    {
        co::MutexGuard g(mtx);
        ++v;
    }

    int* p = (int*) pool.pop();
    if (!p) p = new int(atomic_inc(&n));
    pool.push(p);
}

void f() {
    co::sleep(32);
    LOG << "s: " << co::scheduler_id() << " c: " << co::coroutine_id();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    FLG_cout = true;

    // print scheduler pointers
    auto& s = co::schedulers();
    for (size_t i = 0; i < s.size(); ++i) {
        LOG << "i: " << (void*)s[i] << ", " << (void*)co::next_scheduler();
    }

    for (int i = 0; i < 8; ++i) go(f1);
    go([&]() {
        bool r = ev.wait(50);
        LOG << "f2() r: " << r;
    });

    sleep::ms(100);
    go([&]() {
        LOG << "f3()";
        ev.signal();
    });
    
    LOG << "co::Event wait in non-coroutine beg: " << now::ms();
    ev.wait(200);
    LOG << "co::Event wait in non-coroutine end: " << now::ms();
    sleep::ms(200);

    LOG << "v: " << v;
    LOG << "n: " << n;

    for (int i = 0; i < 32; ++i) go(f);

    sleep::ms(100);
    return 0;
}
