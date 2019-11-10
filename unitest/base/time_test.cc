#include "base/unitest.h"
#include "base/def.h"
#include "base/time.h"
#include "base/str.h"

namespace test {

DEF_test(time) {
    DEF_case(mono) {
        int64 us = now::us();
        int64 ms = now::ms();
        EXPECT_GT(us, 0);
        EXPECT_GT(ms, 0);

        for (int i = 0; i < 3; i++) {
            int64 x = now::us();
            int64 y = now::us();
            EXPECT_LE(x, y);
        }
    }

    DEF_case(str) {
        fastring ymdhms = now::str("%Y%m%d%H%M%S");
        fastring ymd = now::str("%Y%m%d");
        fastring y = now::str("%Y");
        fastring m = now::str("%m");
        fastring d = now::str("%d");
        fastring H = now::str("%H");
        fastring M = now::str("%M");
        fastring S = now::str("%S");

        EXPECT_NE(y, fastring());
        EXPECT_NE(m, fastring());
        EXPECT_NE(d, fastring());
        EXPECT_NE(H, fastring());
        EXPECT_NE(M, fastring());
        EXPECT_NE(S, fastring());
        EXPECT_EQ(ymdhms, y + m + d + H + M + S);
        EXPECT_EQ(ymd, y + m + d);
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
