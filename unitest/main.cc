#include "co/unitest.h"
#include "co/co.h"

DEC_uint32(co_sched_num);

int main(int argc, char** argv) {
    flag::init(argc, argv);
    FLG_co_sched_num = 8;
    co::init();
    unitest::run_all_tests();
    co::exit();
    return 0;
}
