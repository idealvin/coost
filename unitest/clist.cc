#include "co/unitest.h"
#include "co/clist.h"

namespace test {

DEF_test(clist) {
    co::clist l;
    co::clink a;
    co::clink b;
    co::clink c;
    co::clink d;
    co::clink e;

    DEF_case(empty) {
        EXPECT_EQ(l.front(), l.null);
        EXPECT_EQ(l.back(), l.null);
        EXPECT_EQ(l.pop_front(), l.null);
        EXPECT_EQ(l.pop_back(), l.null);
        EXPECT(l.empty());
    }

    DEF_case(push_pop) {
        {
            l.push_front(&a);
            EXPECT_EQ(l.front(), &a);
            EXPECT_EQ(l.back(), &a);
            EXPECT_EQ(a.prev, &a);
            EXPECT_EQ(a.next, l.null);
        }
        {
            l.clear();
            EXPECT(l.empty());
            l.push_back(&a);
            EXPECT_EQ(l.pop_front(), &a);
            EXPECT(l.empty());

            l.push_back(&a);
            EXPECT_EQ(l.pop_back(), &a);
            EXPECT(l.empty());
        }
        {
            // d->c->b->a
            l.clear();
            l.push_front(&a);
            l.push_front(&b);
            l.push_front(&c);
            l.push_front(&d);
            EXPECT_EQ(l.front(), &d);
            EXPECT_EQ(l.back(), &a);
            EXPECT_EQ(d.next, &c);
            EXPECT_EQ(c.next, &b);
            EXPECT_EQ(b.next, &a);
            EXPECT_EQ(a.next, l.null);
            EXPECT_EQ(d.prev, &a);
            EXPECT_EQ(c.prev, &d);
            EXPECT_EQ(b.prev, &c);
            EXPECT_EQ(a.prev, &b);
        }
        {
            co::clink* x;
            x = l.pop_back();
            EXPECT_EQ(x, &a);
            EXPECT_EQ(l.back(), &b);

            x = l.pop_front();
            EXPECT_EQ(x, &d);
            EXPECT_EQ(l.front(), &c);

            l.pop_back();
            l.pop_front();
            EXPECT(l.empty());
        }
        {
            // a->b->c->d
            l.clear();
            l.push_back(&a);
            l.push_back(&b);
            l.push_back(&c);
            l.push_back(&d);
            EXPECT_EQ(l.front(), &a);
            EXPECT_EQ(l.back(), &d);
            EXPECT_EQ(a.next, &b);
            EXPECT_EQ(b.next, &c);
            EXPECT_EQ(c.next, &d);
            EXPECT_EQ(d.next, l.null);
            EXPECT_EQ(a.prev, &d);
            EXPECT_EQ(b.prev, &a);
            EXPECT_EQ(c.prev, &b);
            EXPECT_EQ(d.prev, &c);
        }
        {
            l.clear();

            co::clist m;
            l.push_back(m);
            EXPECT(l.empty());
            l.push_front(m);
            EXPECT(l.empty());

            m.push_back(&a);
            m.push_back(&b);
            l.push_back(m);
            EXPECT(m.empty())
            EXPECT(l.front() == &a);
            EXPECT(l.back() == &b);

            l.clear();
            m.push_back(&a);
            m.push_back(&b);
            l.push_front(m);
            EXPECT(l.front() == &a);
            EXPECT(l.back() == &b);

            // a,b,c,d
            EXPECT(m.empty())
            m.push_back(&c);
            m.push_back(&d);
            l.push_back(m);
            EXPECT(l.front() == &a);
            EXPECT(l.back() == &d);
            EXPECT(b.next == &c);
            EXPECT(c.prev == &b);
            EXPECT(a.prev == &d);
            EXPECT(d.next == l.null);

            // c,d,a,b
            l.pop_back();
            l.pop_back();
            EXPECT(m.empty())
            m.push_back(&c);
            m.push_back(&d);
            l.push_front(m);
            EXPECT(l.front() == &c);
            EXPECT(l.back() == &b);
            EXPECT(d.next == &a);
            EXPECT(a.prev == &d);
            EXPECT(c.prev == &b);
            EXPECT(b.next == l.null);
        }
    }

    DEF_case(erase) {
        l.clear();
        l.push_back(&a);
        l.push_back(&b);
        l.push_back(&c);
        l.push_back(&d);
        l.push_back(&e);
        EXPECT_EQ(l.front(), &a);
        EXPECT_EQ(l.back(), &e);
        EXPECT_EQ(e.next, l.null);

        l.erase(&e);
        EXPECT_EQ(l.front(), &a);
        EXPECT_EQ(l.back(), &d);
        EXPECT_EQ(d.next, l.null);

        l.push_back(&e);
        l.erase(&c);
        EXPECT_EQ(b.next, &d);
        EXPECT_EQ(d.prev, &b);

        l.erase(&a);
        EXPECT_EQ(l.front(), &b);
        EXPECT_EQ(l.back(), &e);

        l.erase(&b);
        l.erase(&d);
        l.erase(&e);
        EXPECT(l.empty());
    }

    DEF_case(move) {
        l.clear();
        l.push_back(&a);
        l.push_back(&b);
        l.push_back(&c);
        l.push_back(&d);
        l.push_back(&e);

        EXPECT_EQ(l.front(), &a);
        l.move_front(&a);
        EXPECT_EQ(l.front(), &a);
        EXPECT_EQ(l.back(), &e);

        l.move_front(&e);
        EXPECT_EQ(l.front(), &e);
        EXPECT_EQ(l.back(), &d);
        EXPECT_EQ(e.next, &a);
        EXPECT_EQ(a.prev, &e);
        EXPECT_EQ(d.next, l.null);

        l.move_back(&e);
        EXPECT_EQ(l.front(), &a);
        EXPECT_EQ(l.back(), &e);
        EXPECT_EQ(e.next, l.null);
        EXPECT_EQ(e.prev, &d);
        EXPECT_EQ(a.prev, &e);

        l.move_back(&e);
        EXPECT_EQ(l.front(), &a);
        EXPECT_EQ(l.back(), &e);

        // c a b d e
        l.move_front(&c);
        EXPECT_EQ(l.front(), &c);
        EXPECT_EQ(c.next, &a);
        EXPECT_EQ(c.prev, &e);
        EXPECT_EQ(a.prev, &c);
        EXPECT_EQ(b.next, &d);
        EXPECT_EQ(d.prev, &b);

        // c a d e b
        l.move_back(&b);
        EXPECT_EQ(l.front(), &c);
        EXPECT_EQ(l.back(), &b);
        EXPECT_EQ(c.prev, &b);
        EXPECT_EQ(a.next, &d);
        EXPECT_EQ(d.prev, &a);
        EXPECT_EQ(b.next, l.null);
        EXPECT_EQ(b.prev, &e);
        EXPECT_EQ(e.next, &b);
    }

    DEF_case(swap) {
        l.clear();
        l.push_back(&a);
        co::clist m;
        m.swap(l);
        EXPECT(l.empty());
        EXPECT(m.front() == &a);
    }

    DEF_case(for_each) {
        l.clear();
        l.push_back(&a);
        l.push_back(&b);
        l.push_back(&c);

        co::clist m;
        l.for_each([&m](co::clink* c) {
            m.push_back(c);
        });
        EXPECT(m.pop_front() == &a);
        EXPECT(m.pop_front() == &b);
        EXPECT(m.pop_front() == &c);
        EXPECT(m.empty());
    }
}

} // test
