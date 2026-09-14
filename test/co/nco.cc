#include "co/co.h"
#include "co/flag.h"

DEF_uint32(n, 1000000, "coroutine number");
DEF_uint32(t, 10, "seconds to sleep in coroutines");

int main(int argc, char** argv) {
    flag::parse(argc, argv);

    for (int i = 0; i < FLG_n; ++i) {
        go([](){
            co::sleep(FLG_t * 1000);
        });
    }

    co::sleep(FLG_t * 2000);
    return 0;
}
