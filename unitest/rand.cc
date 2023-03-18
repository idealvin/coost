#include "co/unitest.h"
#include "co/rand.h"

namespace test {

DEF_test(rand) {
    int x = 0, y = 0;
    for (int i = 0; i < 10000; i++) {
        (co::rand() % 5 < 3) ? x++ : y++;
    }

    EXPECT_GT(x, y);
    EXPECT_EQ((x - y + 100) * 10 / 10000, 2);

    fastring s = co::randstr();
    EXPECT_EQ(s.size(), 15);

    s = co::randstr(23);
    EXPECT_EQ(s.size(), 23);

    s = co::randstr("ab", 8);
    EXPECT_EQ(s.size(), 8);
    int v = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        v ^= s[i];
    }
    EXPECT(v == 0 || v == ('a' ^ 'b'));
}

} // test
