#include "co/print.h"
#include "co/closure.h"

void f() {
    co::println("f()");
}

int main(int argc, char** argv) {
    co::closure c(f);
    c();

    co::closure d([]() {
        co::println("[]() {}");
    });
    d();

    co::closure e([](int v) {
        co::println("e called: ", v);
    }, 8);
    e();

    return 0;
}
