#include "co/unitest.h"

int main(int argc, char** argv) {
    unitest::init(argc, argv);
    unitest::run_all_tests();
    return 0;
}
