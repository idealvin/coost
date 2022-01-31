#include "co/unitest.h"
#include "co/co.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    unitest::run_all_tests();
    co::exit();
    return 0;
}
