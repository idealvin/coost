#include "co/unitest.h"
#include "co/def.h"
#include "co/time.h"

namespace test {

DEF_test(time) {
    DEF_case(mono) {
        int64 us = time::mono.us();
        int64 ms = time::mono.ms();
        EXPECT_GT(us, 0);
        EXPECT_GT(ms, 0);

        int64 x = time::mono.us();
        int64 y = time::mono.us();
        EXPECT_LE(x, y);
    }

    DEF_case(str) {
        co::string ymdhms = time::str("%Y%m%d%H%M%S");
        co::string ymd = time::str("%Y%m%d");
        EXPECT(ymdhms.starts_with(ymd));
    }

    DEF_case(sleep) {
        int64 beg = time::mono.ms();
        time::sleep(1);
        int64 end = time::mono.ms();
        EXPECT_GE(end - beg, 1);
    }

    DEF_case(timer) {
        time::timer t;
        time::sleep(1);
        int64 us = t.us();
        EXPECT_GE(us, 1000);
    }
}

} // namespace test
