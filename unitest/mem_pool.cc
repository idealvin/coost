#include "co/unitest.h"
#include "co/mem_pool.h"

namespace test {

DEF_test(mem_pool) {
    DEF_case(b8p32) {
        MemPool<8, 32> p;
        char* p0 = (char*) p.alloc();
        char* p1 = (char*) p.alloc();
        char* p2 = (char*) p.alloc();
        EXPECT_EQ(p1, p0 + 8);
        EXPECT_EQ(p2, p1 + 8);

        p.dealloc(p0);
        p.dealloc(p1);
        p.dealloc(p2);

        char* p3 = (char*) p.alloc();
        EXPECT_EQ(p3, p2 + 8);

        p.dealloc(p3);
        EXPECT_EQ(p.alloc(), p3);
        EXPECT_EQ(p.alloc(), p2);
        EXPECT_EQ(p.alloc(), p1);
        EXPECT_EQ(p.alloc(), p0);

        char* p4 = (char*) p.alloc();
        char* p5 = (char*) p.alloc();
        EXPECT_NE(p5, p0);
        EXPECT_NE(p5, p1);
        EXPECT_NE(p5, p2);
        EXPECT_NE(p5, p3);
        EXPECT_EQ(p5, p4 + 8);
    }
}

} // namespace test
