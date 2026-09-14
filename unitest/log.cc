#include "co/unitest.h"
#include "co/log.h"

namespace test {

DEF_test(log) {
    const char a[] = "abc";
    const char b[] = "a/bc";
    const char c[] = "a\\bc";
    EXPECT_EQ(log::xx::path_len(a), 3)
    EXPECT_EQ(log::xx::path_len(b), 4)
    EXPECT_EQ(log::xx::path_len(c), 4)
    EXPECT_EQ(log::xx::path_base_len(a), 3)
    EXPECT_EQ(log::xx::path_base_len(b), 2)
    EXPECT_EQ(log::xx::path_base_len(c), 2)
}

} // test
