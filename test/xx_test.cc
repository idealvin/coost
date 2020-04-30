#include "test.h"
#include "base/base.h"
#include <string>

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    COUT << "hello world";

    int N = 100000;
    double d;
    int64 us;

    Timer t;
    for (int i = 0; i < N; ++i) {
        d = std::stod("3.1415926535");
    }
    us = t.us();
    COUT << d << ": " << (us * 1.0 / N);

    t.restart();
    for (int i = 0; i < N; ++i) {
        d = strtod("3.1415926535", 0);
    }
    us = t.us();
    COUT << d << ": " << (us * 1.0 / N);

    return 0;
}
