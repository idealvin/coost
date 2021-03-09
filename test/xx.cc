#include "co/all.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    COUT << "hello world";
    COUT << MAX_UINT64;
    COUT << (uint64)MIN_INT64;
    COUT << "hello world";
    COUT << MAX_INT64;
    COUT << MIN_INT64;

    return 0;
}
