#include "co/unitest.h"
#include "co/co.h"

DEC_uint32(co_sched_num);
DEC_bool(noco);

int main(int argc, char** argv) {
    flag::init(argc, argv);
    if (!FLG_noco) {
        if (FLG_co_sched_num > 8) FLG_co_sched_num = 8;
        co::init();
    }

    unitest::run_all_tests();

    if (!FLG_noco) co::exit();
    return 0;
}
