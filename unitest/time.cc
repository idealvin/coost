#include "co/unitest.h"
#include "co/def.h"
#include "co/time.h"

namespace test {

DEF_test(time) {
    DEF_case(mono) {
        int64 us = co::mono_time.us();
        int64 ms = co::mono_time.ms();
        EXPECT_GT(us, 0);
        EXPECT_GT(ms, 0);

        int64 x = co::mono_time.us();
        int64 y = co::mono_time.us();
        EXPECT_LE(x, y);
    }

    DEF_case(str) {
        co::string ymdhms = co::now.str("%Y%m%d%H%M%S");
        co::string ymd = co::now.str("%Y%m%d");
        EXPECT(ymdhms.starts_with(ymd));
    }

    DEF_case(sleep) {
        int64 beg = co::mono_time.ms();
        time::sleep(1);
        int64 end = co::mono_time.ms();
        EXPECT_GE(end - beg, 1);
    }

    DEF_case(timer) {
        co::timer t;
        time::sleep(1);
        int64 us = t.us();
        EXPECT_GE(us, 1000);
    }
}

} // namespace test
