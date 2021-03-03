#include "co/unitest.h"
#include "co/array.h"

namespace test {

DEF_test(array) {
    DEF_case(cap0) {
        int i = 0;
        Array a(0);
        EXPECT_EQ(a.size(), 0);

        a.push_back(&i);
        EXPECT_EQ(a.size(), 1);
        EXPECT_EQ(a.front(), &i);
        EXPECT_EQ(a[0], &i);
        EXPECT_EQ(a.back(), &i);

        EXPECT_EQ(a.pop_back(), &i);
        EXPECT_EQ(a.size(), 0);
    }

    DEF_case(cap7) {
        int i = 0, k = 0;
        Array a(7);
        EXPECT_EQ(a.size(), 0);

        a.push_back(&i);
        a.push_back(&i);
        a.push_back(&i);
        a.push_back(&i);
        a.push_back(&k);
        a.push_back(&k);
        a.push_back(&k);
        a.push_back(&k);

        EXPECT_EQ(a.size(), 8);
        EXPECT_EQ(a.pop_back(), &k);
        EXPECT_EQ(a.size(), 7);
    }
}

} // namespace test
