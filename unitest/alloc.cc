#include "co/unitest.h"
#include "co/alloc.h"

namespace test {

DEF_test(alloc) {
    DEF_case(static) {
        void* p = co::alloc_static(24);
        EXPECT_NE(p, (void*)0);

        *(uint32*)p = 7;
        EXPECT_EQ(*(uint32*)p, 7);

        int* x = co::new_static<int>(7);
        EXPECT_NE(x, (void*)0);
        EXPECT_EQ(*x, 7);
    }

    DEF_case(fixed) {
        void* p = co::alloc_fixed(8);
        EXPECT_NE(p, (void*)0);

        *(uint32*)p = 7;
        EXPECT_EQ(*(uint32*)p, 7);

        void* x = p;
        co::free_fixed(p, 8);

        p = co::alloc_fixed(72);
        EXPECT_NE(p, (void*)0);
        EXPECT_EQ(p, x);
        co::free_fixed(p, 72);

        p = co::alloc_fixed(2048);
        EXPECT_EQ(p, x);
        co::free_fixed(p, 2048);

        p = co::alloc_fixed(4096);
        EXPECT_NE(p, (void*)0);
        EXPECT_NE(p, x);
        co::free_fixed(p, 4096);

        int* v = co::new_fixed<int>(7);
        EXPECT_EQ(*v, 7);
        co::delete_fixed(v);
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
        p = co::realloc(p, 4 * 1024, 8 * 1024);
        EXPECT_EQ(p, x);
        EXPECT_EQ(*(uint32*)p, 7);

        p = co::realloc(p, 8 * 1024, 32 * 1024);
        EXPECT_EQ(p, x);

        p = co::realloc(p, 32 * 1024, 64 * 1024);
        EXPECT_EQ(p, x);

        p = co::realloc(p, 64 * 1024, 68 * 1024);
        EXPECT_NE(p, x);
        EXPECT_EQ(*(uint32*)p, 7);

        x = p;
        p = co::realloc(p, 68 * 1024, 128 * 1024);
        EXPECT_EQ(p, x);
        co::free(p, 128 * 1024);

        p = co::alloc(96 * 1024);
        EXPECT_NE(p, (void*)0);
        *(uint32*)p = 7;
        EXPECT_EQ(*(uint32*)p, 7);

        x = p;
        p = co::realloc(p, 96 * 1024, 128 * 1024);
        EXPECT_EQ(p, x);
        EXPECT_EQ(*(uint32*)p, 7);

        p = co::realloc(p, 128 * 1024, 256 * 1024);
        EXPECT_NE(p, (void*)0);
        EXPECT_EQ(*(uint32*)p, 7);
        co::free(p, 256 * 1024);
    }
}

} // namespace test
