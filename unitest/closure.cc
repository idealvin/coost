#include "co/unitest.h"
#include "co/closure.h"

namespace test {

int g_closure_count = 0;

static void fa() {
    ++g_closure_count;
}

static void fb(int v) {
    g_closure_count += v;
}

DEF_test(closure) {
    DEF_case(once_closure) {
        {
            co::once_closure c(fa);
            EXPECT_EQ(c._p, (void*)fa)
            c();
            EXPECT_EQ(c._p, nullptr)
            EXPECT_EQ(g_closure_count, 1)
            c();
            EXPECT_EQ(g_closure_count, 1)
        }
        {
            co::once_closure c(fb, 2);
            EXPECT_NE(c._p, (void*)fb)
            c();
            EXPECT_EQ(c._p, nullptr)
            EXPECT_EQ(g_closure_count, 3)
            c();
            EXPECT_EQ(g_closure_count, 3)
        }
        {
            co::once_closure c(fa);
            co::once_closure d(std::move(c));
            EXPECT_EQ(c._p, nullptr)
            EXPECT_EQ(d._p, (void*)fa)
        }
    }

    DEF_case(closure) {
        {
            co::closure c(fa);
            EXPECT_EQ(c._p, (void*)fa)
            c();
            EXPECT_EQ(c._p, (void*)fa)
            EXPECT_EQ(g_closure_count, 4)
            c();
            EXPECT_EQ(g_closure_count, 5)
        }
        {
            co::closure c(fb, 2);
            EXPECT_NE(c._p, (void*)fb)
            c();
            EXPECT_NE(c._p, nullptr)
            EXPECT_EQ(g_closure_count, 7)
            c();
            EXPECT_EQ(g_closure_count, 9)
        }
        {
            co::closure c(fb, 2);
            co::closure d(std::move(c));
            EXPECT_EQ(c._p, nullptr)
            EXPECT_NE(d._p, nullptr)
            c();
            EXPECT_EQ(g_closure_count, 9)
            d();
            EXPECT_EQ(g_closure_count, 11)

            co::closure e;
            e = std::move(c);
            EXPECT_EQ(e._p, nullptr)

            e = std::move(d);
            EXPECT_EQ(d._p, nullptr)
            EXPECT_NE(e._p, nullptr)

            e();
            EXPECT_EQ(g_closure_count, 13)
        }
    }
}

} // test
