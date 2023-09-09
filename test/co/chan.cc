#include "co/co.h"
#include "co/cout.h"

void f() {
    co::chan<int> ch;
    go([ch]() { ch << 7; });
    int v = 0;
    ch >> v;
    co::print("v: ", v);
}

void g() {
    co::chan<int> ch(32, 500);
    go([ch]() {
        ch << 7;
        if (!ch.done()) co::print("write to channel timeout..");
    });

    int v = 0;
    ch >> v;
    if (ch.done()) co::print("v: ", v);
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    f();
    g();
    return 0;
}
