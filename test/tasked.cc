#include "co/tasked.h"
#include "co/print.h"
#include "co/time.h"

void f() {
    co::println("f(): ", co::now.str());
}

void g() {
    co::println("g(): ", co::now.str());
}

int main(int argc, char** argv) {
    co::println("now: ", co::now.str());

    co::tasked s;
    s.run_in(f, 0);
    s.run_in(f, 1);
    s.run_in(f, 2);
    s.run_in(f, 0);
    s.run_every(g, 3);
    s.run_at(f, 17, 12, 59);
    s.run_daily(f, 5, 18, 0);

    time::sleep(7000);
    s.stop();

    return 0;
}
