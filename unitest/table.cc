#include "co/unitest.h"
#include "co/table.h"

namespace test {

DEF_test(table) {
    co::table<int> tb(3, 3); // 8 x 8
    EXPECT_EQ(tb[0], 0);
    EXPECT_EQ(tb[7], 0);
    tb[3] = 3;
    EXPECT_EQ(tb[3], 3);

    tb[17] = 17;
    EXPECT_EQ(tb[8], 0);
    EXPECT_EQ(tb[15], 0);
    EXPECT_EQ(tb[16], 0);
    EXPECT_EQ(tb[17], 17);
}

}
