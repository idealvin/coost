#include "co/co.h"
#include "co/print.h"
#include "co/flag.h"

DEF_uint32(n, 8, "coroutine number");

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    
    co::wait_group wg;
    wg.add(FLG_n);

    for (uint32 i = 0; i < FLG_n; ++i) {
        go([wg]() {
            co::println("sched: ", co::sched_id(), " co: ", co::coroutine_id());
            wg.done();
        });
    }

    wg.wait();
    return 0;
}
