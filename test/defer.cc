#include "co/defer.h"
#include "co/cout.h"
#include "co/time.h"

void f(int x, int y) {
    CLOG << (x + y);
}

void f() {
    Timer t;
    defer(CLOG << "time elapse: " << t.us() << " us");
    CLOG << "hello f()";
}

int main(int argc, char** argv) {
    defer(CLOG << "hello world");
    defer(CLOG << "hello again");
    defer(f(1, 1); f(1, 3));
    f();
    f();
    return 0;
}
