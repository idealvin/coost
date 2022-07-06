#include "co/unitest.h"
#include "co/mem.h"

namespace test {

DEF_test(mem) {
    DEF_case(static) {
        void* p = co::static_alloc(24);
        EXPECT_NE(p, (void*)0);

        *(uint32*)p = 7;
        EXPECT_EQ(*(uint32*)p, 7);

        int* x = co::static_new<int>(7);
        EXPECT_NE(x, (void*)0);
        EXPECT_EQ(*x, 7);
    }

    DEF_case(small) {
        void* p = co::alloc(2048);
        EXPECT_NE(p, (void*)0);

        *(uint32*)p = 7;
        EXPECT_EQ(*(uint32*)p, 7);
        co::free(p, 2048);

        void* x = p;
        p = co::alloc(8);
        EXPECT_EQ(p, x);
        co::free(p, 8);

        p = co::alloc(72);
        EXPECT_EQ(p, x);
        co::free(p, 72);

        p = co::alloc(4096);
        EXPECT_NE(p, (void*)0);
        EXPECT_NE(p, x);
        co::free(p, 4096);

        int* v = co::make<int>(7);
        EXPECT_EQ(*v, 7);
        co::del(v);
    }

    DEF_case(realloc) {
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

    DEF_case(unique_ptr) {
        co::unique_ptr<int> p;
        EXPECT(p == NULL);
        EXPECT(!p);

        p.reset(co::make<int>(7));
        EXPECT_EQ(*p, 7);
        *p = 3;
        EXPECT_EQ(*p, 3);

        co::unique_ptr<int> q(co::make<int>(7));
        EXPECT_EQ(*q, 7);

        q = std::move(p);
        EXPECT_EQ(*q, 3);
        EXPECT(p == NULL);

        int* x = q.release();
        EXPECT_EQ(*x, 3);
        EXPECT(q == NULL);

        p.reset(x);
        EXPECT_EQ(*p, 3);
        EXPECT_EQ(p.get(), x);
    }

    DEF_case(shared_ptr) {
        co::shared_ptr<int> p;
        EXPECT(p == NULL);
        EXPECT(!p);
        EXPECT_EQ(p.use_count(), 0);

        co::shared_ptr<int> q(p);
        EXPECT_EQ(p.use_count(), 0);
        EXPECT_EQ(q.use_count(), 0);

        int* x = co::make<int>(7);
        p.reset(x);
        EXPECT_EQ(*p, 7);
        *p = 3;
        EXPECT_EQ(*p, 3);
        EXPECT_EQ(p.use_count(), 1);
        EXPECT_EQ(q.use_count(), 0);
        EXPECT_EQ(p.get(), x);
        EXPECT(p == x);

        q = p;
        EXPECT_EQ(p.use_count(), 2);
        EXPECT_EQ(q.use_count(), 2);
        EXPECT_EQ(*q, 3);

        p.reset();
        EXPECT(p == NULL);
        EXPECT_EQ(q.use_count(), 1);
        EXPECT_EQ(*q, 3);

        p.swap(q);
        EXPECT(q == NULL);
        EXPECT_EQ(p.use_count(), 1);
        EXPECT_EQ(*p, 3);

        q = std::move(p);
        EXPECT(p == NULL);
        EXPECT_EQ(q.use_count(), 1);
        EXPECT_EQ(*q, 3);
    }
}

} // namespace test
