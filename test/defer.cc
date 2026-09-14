#include "co/defer.h"
#include "co/print.h"
#include "co/time.h"

void f(int sn, int x, int y) {
    co::println(sn, ": ", x + y);
}

void f() {
    co::timer t;
    defer(
        co::println("time elapse: ", t.us(), "us")
    );
    co::println("hello f()");
}

int main(int argc, char** argv) {
    defer(co::println("hello world"));
    defer(co::println("hello again"));
    defer(
        f(1, 1, 1);
        f(2, 1, 3);
    );
    f();
    f();
    return 0;
}
