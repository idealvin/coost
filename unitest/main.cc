#include "co/unitest.h"
#include "co/co.h"

DEC_uint32(co_sched_num);

int main(int argc, char** argv) {
    FLG_co_sched_num = 4;
    co::init(argc, argv);
    unitest::run_all_tests();
    co::exit();
    return 0;
}
