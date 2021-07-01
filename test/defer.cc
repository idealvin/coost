#include "co/defer.h"
#include "co/log.h"

void f(int x, int y) {
    COUT << (x + y);
}

int main(int argc, char** argv) {
    defer(COUT << "hello world");
    defer(COUT << "hello again");
    defer(f(1, 1); f(1, 3));
    return 0;
}
