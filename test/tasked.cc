#include "co/log.h"
#include "co/time.h"
#include "co/tasked.h"

void f() {
    COUT << "f(): " << now::str();
}

void g() {
    COUT << "g(): " << now::str();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    COUT << "now: " << now::str();

    Tasked s;
    s.run_in(f, 0);
    s.run_in(f, 1);
    s.run_in(f, 2);
    s.run_in(f, 0);
    s.run_every(g, 3);
    s.run_daily(f, 5, 18, 0);

    while (1) sleep::sec(1024);
    return 0;
}
