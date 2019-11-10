#include "base/unitest.h"
#include "base/random.h"

namespace test {

DEF_test(random) {
    Random r;
    int x = 0, y = 0;
    for (int i = 0; i < 10000; i++) {
        (r.next() % 5 < 3) ? x++ : y++;
    }

    EXPECT_GT(x, y);
    EXPECT_EQ((x - y + 100) * 10 / 10000, 2);
}

} // namespace test
