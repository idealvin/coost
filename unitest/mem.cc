#include "co/unitest.h"
#include "co/mem.h"

namespace test {
namespace mem {

struct DoubleLink {
    DoubleLink* next;
    DoubleLink* prev;
};

typedef DoubleLink* list_t;

inline void list_push_front(list_t& l, DoubleLink* node) {
    if (l) {
        node->next = l;
        node->prev = l->prev;
        l->prev = node;
        l = node;
    } else {
        node->next = NULL;
        node->prev = node;
        l = node;
    }
}

// move heading node to the back
inline void list_move_head_back(list_t& l) {
    const auto head = l->next;
    l->prev->next = l;
    l->next = NULL;
    l = head;
}

// erase non-heading node
inline void list_erase(list_t& l, DoubleLink* node) {
    node->prev->next = node->next;
    const auto x = node->next ? node->next : l;
    x->prev = node->prev;
}

inline size_t list_size(list_t& l) {
    size_t x = 0;
    for (DoubleLink* p = l; p; p = p->next) ++x;
    return x;
}

} // mem

DEF_test(mem) {
    DEF_case(list) {
        mem::DoubleLink* h = 0;
        mem::list_t& l = h;
        EXPECT_EQ(mem::list_size(l), 0);

        mem::DoubleLink a;
        mem::DoubleLink b;
        mem::DoubleLink c;

        mem::list_push_front(l, &a);
        EXPECT_EQ(mem::list_size(l), 1);
        EXPECT_EQ(l->next, (void*)0);
        EXPECT_EQ(l->prev, &a);

        mem::list_push_front(l, &b);
        mem::list_push_front(l, &c);
        EXPECT_EQ(l, &c);
        EXPECT_EQ(mem::list_size(l), 3);
        EXPECT_EQ(l->prev, &a);

        mem::list_move_head_back(l);
        EXPECT_EQ(l, &b);
        EXPECT_EQ(mem::list_size(l), 3);

        mem::list_erase(l, &a);
        EXPECT_EQ(mem::list_size(l), 2);
        EXPECT_EQ(l->next, &c);

        mem::list_erase(l, &c);
        EXPECT_EQ(mem::list_size(l), 1);
        EXPECT_EQ(l, &b);
    }

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
