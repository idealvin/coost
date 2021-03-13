#include "co/unitest.h"
#include "co/mem_pool.h"

namespace test {

DEF_test(mem_pool) {
    DEF_case(cap32) {
        MemPool p(8, 32);
        uint32 i0 = p.alloc();
        uint32 i1 = p.alloc();
        uint32 i2 = p.alloc();
        EXPECT_EQ(i0, 0);
        EXPECT_EQ(i1, 1);
        EXPECT_EQ(i2, 2);

        uint64* p0 = (uint64*) p.at(i0);
        uint64* p1 = (uint64*) p.at(i1);
        uint64* p2 = (uint64*) p.at(i2);
        EXPECT_EQ(p1, p0 + 1);
        EXPECT_EQ(p2, p0 + 2);
        
        p.dealloc(i0);
        p.dealloc(i1);
        p.dealloc(i2);
        EXPECT_EQ(p.alloc(), 0);
    }

    DEF_case(cap0) {
        MemPool p(16, 0);
        uint32 i0 = p.alloc();
        uint32 i1 = p.alloc();
        uint32 i2 = p.alloc();
        EXPECT_EQ(i0, 0);
        EXPECT_EQ(i1, 1);
        EXPECT_EQ(i2, 2);

        char* p0 = (char*) p.at(i0);
        char* p1 = (char*) p.at(i1);
        char* p2 = (char*) p.at(i2);
        EXPECT_EQ(p1, p0 + 16);
        EXPECT_EQ(p2, p0 + 32);
        
        p.dealloc(i0);
        p.dealloc(i1);
        p.dealloc(i2);
        EXPECT_EQ(p.alloc(), 0);
    }
}

} // namespace test
