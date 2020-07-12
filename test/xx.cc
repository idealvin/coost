#include "test.h"
#include "co/all.h"

#include "co/co.h"
#include "co/log.h"
#include "co/time.h"

co::Event ev;

DEF_bool(wait_alway, true, "false: wait for timeout, true wait alway");
void f2(){
    CLOG << "f2() start";
    bool r = true;
    if (FLG_wait_alway) {
        ev.wait();
    } else {
        r = ev.wait(1*1000);
    }
    CLOG << "f2() end: " << r;
}

void f3() {
    CLOG << "f3() start";
    ev.signal();
    CLOG << "f3() end";
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();
    go(f2);
    sleep::ms(100);
    go(f3);

    sleep::ms(2*1000);
    return 0;
}