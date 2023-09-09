#include "co/unitest.h"
#include "co/rand.h"

namespace test {

DEF_test(rand) {
    int x = 0, y = 0;
    for (int i = 0; i < 10000; i++) {
        (co::rand() % 5 < 3) ? x++ : y++;
    }

    EXPECT_GT(x, y);
    double d = x * 1.0 / y;
    EXPECT(1.4 < d && d < 1.6);

    EXPECT(co::randstr(0).empty());
    EXPECT(co::randstr(-1).empty());
    EXPECT(co::randstr("abc", 0).empty());
    EXPECT(co::randstr("abc", -1).empty());
    EXPECT(co::randstr(NULL, 23).empty());
    EXPECT(co::randstr("", 23).empty());

    fastring s = co::randstr();
    EXPECT_EQ(s.size(), 15);

    s = co::randstr(23);
    EXPECT_EQ(s.size(), 23);

    EXPECT_EQ(co::randstr("x", 6), fastring(6, 'x'));

    s = co::randstr("ab", 8);
    EXPECT_EQ(s.size(), 8);
    int v = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        v ^= s[i];
    }
    EXPECT(v == 0 || v == ('a' ^ 'b'));

    s = co::randstr("0-2", 8);
    EXPECT(s.contains('0') || s.contains('1') || s.contains('2'));
    EXPECT(!s.contains('a') && !s.contains('b') && !s.contains('3'));
}

} // test
