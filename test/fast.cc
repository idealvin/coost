#include "co/cout.h"
#include "co/flag.h"
#include "co/time.h"
#include "co/benchmark.h"

DEF_uint64(beg, 1000, "beg");
DEF_uint64(end, 9999, "end");
DEF_double(f, 3.14159, "double");

char buf[32] = { 0 };

BM_group(uint64_to_string) {
    BM_add(snprintf)(
        for (uint64 i = FLG_beg; i < FLG_end; i++) {
            snprintf(buf, 32, "%llu", i);
        }
    )
    BM_use(buf);

    BM_add(u64toa)(
        for (uint64 i = FLG_beg; i < FLG_end; i++) {
            fast::u64toa(i, buf);
        }
    )
    BM_use(buf);
}

BM_group(uint64_to_hex) {
    BM_add(snprintf)(
        for (uint64 i = FLG_beg; i < FLG_end; i++) {
            snprintf(buf, 32, "0x%llx", i);
        }
    )
    BM_use(buf);

    BM_add(u64toh)(
        for (uint64 i = FLG_beg; i < FLG_end; i++) {
            fast::u64toh(i, buf);
        }
    )
    BM_use(buf);
}

BM_group(double_to_string) {
    int r;
    BM_add(snprintf)(
        r = snprintf(buf, 32, "%.7g", FLG_f);
    )
    BM_use(r);
    BM_use(buf);

    BM_add(dtoa)(
        r = fast::dtoa(FLG_f, buf);
    )
    BM_use(r);
    BM_use(buf);
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    bm::run_benchmarks();
    return 0;
}
