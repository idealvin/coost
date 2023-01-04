#include "co/unitest.h"
#include "co/co.h"

namespace test {

int gc = 0;
int gd = 0;

struct TestChan {
    explicit TestChan(int v = 0) : v(v) {
        atomic_inc(&gc, mo_relaxed);
    }

    TestChan(const TestChan& c) : v(c.v) {
        atomic_inc(&gc, mo_relaxed);
    }

    TestChan(TestChan&& c) : v(c.v) {
        c.v = 0;
        atomic_inc(&gc, mo_relaxed);
    }

    ~TestChan() {
        if (v) v = 0;
        atomic_inc(&gd, mo_relaxed);
    }

    int v;
};

DEF_test(co) {
    int v = 0;

    DEF_case(wait_group) {
        co::wait_group wg;
        wg.add(8);
        for (int i = 0; i < 7; ++i) {
            go([wg, &v]() {
                atomic_inc(&v);
                wg.done();
            });
        }

        std::thread([wg, &v]() {
            atomic_inc(&v);
            wg.done();
        }).detach();

        wg.wait();
        EXPECT_EQ(v, 8);
        v = 0;
    }

    DEF_case(event) {
        {
            co::event ev;
            co::wait_group wg(2);

            go([wg, ev, &v]() {
                ev.wait();
                if (v == 1) v = 2;
                wg.done();
            });

            go([wg, ev, &v]() {
                if (v == 0) {
                    v = 1;
                    ev.signal();
                }
                wg.done();
            });

            wg.wait();
            EXPECT_EQ(v, 2);
            v = 0;

            ev.signal();
            EXPECT_EQ(ev.wait(1), true);
            EXPECT_EQ(ev.wait(1), false);
        }
        {
            co::event ev(true, true); // manual reset
            co::wait_group wg(1);

            go([wg, ev, &v]() {
                if (ev.wait(32)) {
                    ev.reset();
                    v = 1;
                }
                wg.done();
            });

            wg.wait();
            EXPECT_EQ(v, 1);
            v = 0;

            EXPECT_EQ(ev.wait(1), false);
            ev.signal();
            EXPECT_EQ(ev.wait(1), true);
            EXPECT_EQ(ev.wait(1), true);

            ev.reset();
            EXPECT_EQ(ev.wait(1), false);
        }
    }

    DEF_case(mutex) {
        co::mutex m;
        co::wait_group wg;
        wg.add(8);

        m.lock();
        EXPECT_EQ(m.try_lock(), false);
        m.unlock();
        EXPECT_EQ(m.try_lock(), true);
        m.unlock();

        for (int i = 0; i < 4; ++i) {
            go([wg, m, &v]() {
                co::mutex_guard g(m);
                ++v;
                wg.done();
            });
        }

        for (int i = 0; i < 4; ++i) {
            std::thread([wg, m, &v]() {
                co::mutex_guard g(m);
                ++v;
                wg.done();
            }).detach();
        }

        wg.wait();
        EXPECT_EQ(v, 8);
        v = 0;
    }

    DEF_case(chan) {
        {
            co::chan<int> ch;
            co::wait_group wg(2);

            go([wg, ch]() {
                ch << 23;
                wg.done();
            });

            go([wg, ch, &v]() {
                ch >> v;
                wg.done();
            });

            wg.wait();
            EXPECT_EQ(v, 23);
            v = 0;
        }

        {
            fastring s("hello");
            fastring t("again");
            auto ps = s.data();
            auto pt = t.data();

            co::chan<fastring> ch(4, 8);
            ch << s;
            ch << std::move(t);
            EXPECT(ch);
            EXPECT(ch.done());

            EXPECT_EQ(s, "hello");
            EXPECT_EQ(t.capacity(), 0);

            fastring x;
            ch >> x;
            EXPECT_EQ(x, "hello");
            EXPECT_NE(x.data(), ps);

            ch >> x;
            EXPECT_EQ(x, "again");
            EXPECT_EQ(x.data(), pt);

            ch << s << s << s << s;
            EXPECT(ch.done());

            ch << s;
            EXPECT(!ch.done());

            ch.close();
            EXPECT(!ch);

            int i = 0;
            do {
                ch >> x;
                if (ch.done()) ++i;
            } while (ch.done());
            EXPECT_EQ(i, 4);
            EXPECT_EQ(x, "hello");
        }

        {
            TestChan x(7);
            co::chan<TestChan> ch(4, 32);
            ch << x;
            EXPECT_EQ(x.v, 7);

            TestChan y;
            ch >> y;
            EXPECT_EQ(y.v, 7);

            ch << std::move(x);
            EXPECT_EQ(x.v, 0);

            y.v = 0;
            ch >> y;
            EXPECT_EQ(y.v, 7);

            // ch is full after this
            ch << y << y << y << y;

            co::wait_group wg(2);
            auto s = co::next_scheduler();
            s->go([ch, wg]() {
                TestChan x(3);
                ch << x;
                wg.done();
            });
            s->go([ch, wg]() {
                TestChan r;
                ch >> r;
                wg.done();
            });
            wg.wait();

            ch >> y >> y >> y;
            EXPECT_EQ(y.v, 7);

            ch >> y;
            EXPECT_EQ(y.v, 3);

            y.v = 0;
            ch >> y;
            EXPECT(!ch.done()); // timeout
            EXPECT_EQ(y.v, 0);

            wg.add(2);
            s->go([ch, wg, &y]() {
                ch >> y;
                wg.done();
            });
            s->go([ch, wg]() {
                ch << TestChan(8);
                wg.done();
            });
            wg.wait();
            EXPECT_EQ(y.v, 8);

            int kk = 0;
            wg.add(2);
            std::thread([ch, wg, &y, &kk]() {
                atomic_store(&kk, 1);
                ch >> y;
                wg.done();
            }).detach();

            go([ch, wg, &kk]() {
                while (atomic_load(&kk) != 1) co::sleep(1);
                ch << TestChan(7);
                wg.done();
            });
            wg.wait();
            EXPECT_EQ(y.v, 7);

            int i = 0;
            do {
                ch >> y;
                if (ch.done()) ++i;
            } while (ch.done());
            EXPECT_EQ(i, 0);

            y.v = 0;
            atomic_store(&kk, 0, mo_relaxed);
            wg.add(1);
            s->go([ch, wg, &y, &kk]() {
                atomic_inc(&kk, mo_relaxed);
                ch >> y;
                wg.done();
            });

            while (kk == 0) co::sleep(1);
            co::sleep(8);
            ch.close();
            EXPECT(!ch);
            wg.wait();
            EXPECT_EQ(y.v, 0);
        }

        {
            TestChan x(7);
            co::chan<TestChan> ch(4, 32);

            int kk = 0;
            co::wait_group wg(2);
            ch << x << x << x << x;

            go([ch, wg, &x, &kk]() {
                atomic_inc(&kk, mo_relaxed);
                ch << x;
                wg.done();
            });

            go([ch, wg, &x, &kk]() {
                atomic_inc(&kk, mo_relaxed);
                ch << x;
                wg.done();
            });

            while (kk != 2) co::sleep(1);
            co::sleep(8);
            ch.close();
            EXPECT(!ch);

            int i = 0;
            do {
                ch >> x;
                if (ch.done()) ++i;
            } while (ch.done());
            EXPECT_EQ(i, 6);
        }

        EXPECT_EQ(gc, gd);
    }

    DEF_case(pool) {
        co::pool p(
            []() { return (void*) new int(0); },
            [](void* p) { delete (int*)p; },
            8192
        );

        int n = co::scheduler_num();
        co::array<int> vi(n, 0);

        co::wait_group wg;
        wg.add(n);

        for (int i = 0; i < n; ++i) {
            go([wg, p, i]() {
                co::pool_guard<int> g(p);
                *g = i;
                wg.done();
            });
        }

        wg.wait();

        wg.add(n);
        for (int i = 0; i < n; ++i) {
            go([wg, p, i, &vi]() {
                int* x = (int*) p.pop();
                vi[i] = *x;
                p.push(x);
                wg.done();
            });
        }

        wg.wait();
        for (size_t i = 0; i < vi.size(); ++i) {
            EXPECT_EQ(vi[i], i);
        }

        p.clear();
    }
}

} // test
