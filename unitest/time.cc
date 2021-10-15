#include "co/unitest.h"
#include "co/def.h"
#include "co/time.h"
#include "co/str.h"

namespace test {

DEF_test(time) {
    DEF_case(mono) {
        int64 us = now::us();
        int64 ms = now::ms();
        EXPECT_GT(us, 0);
        EXPECT_GT(ms, 0);

        int64 x = now::us();
        int64 y = now::us();
        EXPECT_LE(x, y);
    }

    DEF_case(str) {
        fastring ymdhms = now::str("%Y%m%d%H%M%S");
        fastring ymd = now::str("%Y%m%d");
        EXPECT(ymdhms.starts_with(ymd));
    }

    DEF_case(sleep) {
        int64 beg = now::ms();
        sleep::ms(1);
        int64 end = now::ms();
        EXPECT_GE(end - beg, 1);
    }

    DEF_case(timer) {
        Timer timer;
        sleep::ms(1);
        int64 t = timer.us();
        EXPECT_GE(t, 1000);
    }
}

} // namespace test
