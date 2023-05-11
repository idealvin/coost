#include "co/defer.h"
#include "co/cout.h"
#include "co/time.h"

void f(int sn, int x, int y) {
    co::print(sn, ": ", x + y);
}

void f() {
    co::Timer t;
    defer(
        co::print("time elapse: ", t.us(), "us")
    );
    co::print("hello f()");
}

int main(int argc, char** argv) {
    defer(co::print("hello world"));
    defer(co::print("hello again"));
    defer(
        f(1, 1, 1);
        f(2, 1, 3);
    );
    f();
    f();
    return 0;
}
