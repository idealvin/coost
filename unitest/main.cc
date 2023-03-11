#include "co/unitest.h"
#include "co/co.h"

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    unitest::run_tests();
    return 0;
}
