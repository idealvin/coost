#include "co/benchmark.h"
#include "co/string.h"
#include <cinttypes>

DEF_uint64(beg, 1000, "beg");
DEF_uint64(end, 9999, "end");
DEF_double(f, 3.14159, "double");

char buf[32] = { 0 };

BM_group(xtoa) {
    BM_sub_group_begin;
    BM_add(snprintf(%llu)) {
        for (uint64 i = FLG_beg; i < FLG_end; i++) {
            snprintf(buf, 32, "%" PRIu64, i);
        }
    }
    BM_use(buf);

    BM_add(itoa) {
        for (uint64 i = FLG_beg; i < FLG_end; i++) {
            co::itoa(i, buf, 32);
        }
    }
    BM_use(buf);

    BM_sub_group_begin;
    BM_add(snprintf(0x%llx)) {
        for (uint64 i = FLG_beg; i < FLG_end; i++) {
            snprintf(buf, 32, "0x" PRIx64, i);
        }
    }
    BM_use(buf);

    BM_add(utoh) {
        for (uint64 i = FLG_beg; i < FLG_end; i++) {
            co::utoh(i, buf, 32);
        }
    }
    BM_use(buf);

    BM_sub_group_begin;
    int r;
    BM_add(snprintf(%.7g)) {
        r = snprintf(buf, 32, "%.7g", FLG_f);
    }
    BM_use(r);
    BM_use(buf);

    BM_add(dtoa) {
        r = co::dtoa(FLG_f, buf);
    }
    BM_use(r);
    BM_use(buf);
}
