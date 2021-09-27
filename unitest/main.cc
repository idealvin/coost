#include "co/unitest.h"
#include "co/co.h"

DEC_bool(noco);

int main(int argc, char** argv) {
    flag::init(argc, argv);
    if (!FLG_noco) co::init();
    unitest::run_all_tests();
    if (!FLG_noco) co::exit();
    return 0;
}
