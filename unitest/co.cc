#include "co/unitest.h"
#include "co/co.h"

namespace test {

DEF_test(co) {
    int v = 0;

    DEF_case(wait_group) {
        co::WaitGroup wg;
        wg.add(8);
        for (int i = 0; i < 8; ++i) {
            go([wg, &v]() {
                atomic_inc(&v);
                wg.done();
            });
        }

        wg.wait();
        EXPECT_EQ(v, 8);
        v = 0;
    }

    DEF_case(event) {
        co::Event ev;
        co::WaitGroup wg;
        wg.add(2);

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

    DEF_case(channel) {
        co::Chan<int> ch;
        co::WaitGroup wg;
        wg.add(2);

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

    DEF_case(mutex) {
        co::Mutex m;
        co::WaitGroup wg;
        wg.add(8);

        for (int i = 0; i < 8; ++i) {
            go([wg, m, &v]() {
                co::MutexGuard g(m);
                ++v;
                wg.done();
            });
        }

        wg.wait();
        EXPECT_EQ(v, 8);
        v = 0;
    }

    DEF_case(pool) {
        co::Pool p(
            []() { return (void*) new int(0); },
            [](void* p) { delete (int*)p; },
            8192
        );

        int n = co::scheduler_num();
        co::vector<int> vi(n, 0);

        co::WaitGroup wg;
        wg.add(n);

        for (int i = 0; i < n; ++i) {
            go([wg, p, i]() {
                co::PoolGuard<int> g(p);
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
