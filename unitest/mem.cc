#include "co/unitest.h"
#include "co/mem.h"

namespace test {

DEF_test(alloc) {
    DEF_case(static) {
        void* p = co::static_alloc(24);
        EXPECT_NE(p, (void*)0);

        *(uint32*)p = 7;
        EXPECT_EQ(*(uint32*)p, 7);

        int* x = co::static_new<int>(7);
        EXPECT_NE(x, (void*)0);
        EXPECT_EQ(*x, 7);
    }

    DEF_case(fixed) {
        void* p = co::fixed_alloc(8);
        EXPECT_NE(p, (void*)0);

        *(uint32*)p = 7;
        EXPECT_EQ(*(uint32*)p, 7);

        void* x = p;
        co::free(p, 8);

        p = co::fixed_alloc(72);
        EXPECT_NE(p, (void*)0);
        EXPECT_EQ(p, x);
        co::free(p, 72);

        p = co::fixed_alloc(2048);
        EXPECT_EQ(p, x);
        co::free(p, 2048);

        p = co::fixed_alloc(4096);
        EXPECT_NE(p, (void*)0);
        EXPECT_NE(p, x);
        co::free(p, 4096);

        int* v = co::make<int>(7);
        EXPECT_EQ(*v, 7);
        co::del(v);
    }

    DEF_case(deflt) {
        void* p;
        p = co::alloc(48);
        EXPECT_NE(p, (void*)0);
        *(uint32*)p = 7;
        EXPECT_EQ(*(uint32*)p, 7);

        void* x = p;
        p = co::realloc(p, 48, 64);
        EXPECT_EQ(p, x);
        EXPECT_EQ(*(uint32*)p, 7);

        p = co::realloc(p, 64, 128);
        EXPECT_EQ(p, x);

        p = co::realloc(p, 128, 2048);
        EXPECT_EQ(p, x);

        p = co::realloc(p, 2048, 4096);
        EXPECT_NE(p, x);
        EXPECT_EQ(*(uint32*)p, 7);

        x = p;
        p = co::realloc(p, 4096, 8 * 1024);
        EXPECT_EQ(p, x);
        EXPECT_EQ(*(uint32*)p, 7);

        p = co::realloc(p, 8 *1024, 32 * 1024);
        EXPECT_EQ(p, x);

        p = co::realloc(p, 32 * 1024, 64 * 1024);
        EXPECT_EQ(p, x);

        x = p;
        p = co::realloc(p, 64 * 1024, 132 * 1024);
        EXPECT_NE(p, x);
        EXPECT_EQ(*(uint32*)p, 7);

        p = co::realloc(p, 132 * 1024, 256 * 1024);
        EXPECT_EQ(*(uint32*)p, 7);
        co::free(p, 256 * 1024);
    }
}

} // namespace test
