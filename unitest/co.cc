#include "co/unitest.h"
#include "co/co.h"
#include "co/thread.h"

#include "../src/co/buffer.h"
#include "../src/co/idgen.h"
#include "../src/co/sched.h"

namespace test {

int g_x, g_y;

DEF_test(co) {
    DEF_case(Buffer) {
        void* pbuf = 0;
        co::Buffer& buf = *(co::Buffer*)&pbuf;

        EXPECT_EQ(buf.size(), 0);
        EXPECT_EQ(buf.capacity(), 0);

        buf.append("hello world", 11);
        EXPECT_EQ(buf.size(), 11);
        EXPECT_EQ(buf.capacity(), 11);

        buf.append("1234567890", 10);
        EXPECT_EQ(buf.size(), 21);
        EXPECT_GE(buf.capacity(), 21);

        buf.clear();
        EXPECT_EQ(buf.size(), 0);
        EXPECT_GE(buf.capacity(), 21);

        buf.reset();
        EXPECT_EQ(buf.size(), 0);
        EXPECT_EQ(buf.capacity(), 0);
    }

    DEF_case(IdGen) {
        co::IdGen g;
        int x = g.pop();
        EXPECT_EQ(x, 0);

        for (int i = 1; i <= 4095; ++i) {
            x = g.pop();
        }
        EXPECT_EQ(x, 4095);

        x = g.pop();
        EXPECT_EQ(x, 4096);

        g.push(x);
        g.push(x - 1);

        x = g.pop();
        EXPECT_EQ(x, 4095);
        x = g.pop();
        EXPECT_EQ(x, 4096);

        g.push(7);
        x = g.pop();
        EXPECT_EQ(x, 7);

        for (int i = 0; i <= 4096; i++) {
            g.push(i);
        }

        for (int i = 0; i <= (1 << 18); ++i) {
            x = g.pop();
        }

        EXPECT_EQ(x, 1 << 18);
        g.push(x);

        x = g.pop();
        EXPECT_EQ(x, 1 << 18);

        for (int i = 0; i <= (1 << 18); ++i) {
            g.push(i);
        }

        x = g.pop();
        EXPECT_EQ(x, 0);
    }

    int v = 0;

    DEF_case(wait_group) {
        co::atomic_store(&v, 0);
        co::wait_group wg;
        wg.add(8);
        for (int i = 0; i < 7; ++i) {
            go([wg, &v]() {
                co::atomic_inc(&v);
                wg.done();
            });
        }

        std::thread([wg, &v]() {
            co::atomic_inc(&v);
            wg.done();
        }).detach();

        // single waiter
        wg.wait();
        EXPECT_EQ(v, 8);
        v = 0;

        // multiple waiters
        co::wait_group wg2(2);
        wg.add(2);
        go([wg, wg2, &v]{
            wg.wait();
            co::atomic_inc(&v);
            wg2.done();
        });
        std::thread([wg, wg2, &v]{
            wg.wait();
            co::atomic_inc(&v);
            wg2.done();
        }).detach();

        go([wg]{
            wg.done();
        });
        std::thread([wg]{
            wg.done();
        }).detach();

        wg2.wait();
        EXPECT_EQ(v, 2);

        // all done() called before wait 
        wg.add(1);
        wg2.add(1);
        go([wg, wg2, &v]{
            co::atomic_inc(&v, co::mo_relaxed);
            wg.done();
            wg2.done();
        });

        wg2.wait();
        wg.wait();
        EXPECT_EQ(v, 3);
    }

    DEF_case(mutex) {
        co::atomic_store(&v, 0);

        co::mutex m;
        co::wait_group wg;

        m.lock();
        EXPECT_EQ(m.try_lock(), false);
        m.unlock();
        EXPECT_EQ(m.try_lock(), true);
        m.unlock();

        wg.add(16);
        for (int i = 0; i < 12; ++i) {
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
        EXPECT_EQ(v, 16);
        v = 0;

        wg.add(16);
        for (int i = 0; i < 16; ++i) {
            go([wg, m, &v] {
                co::mutex_guard g(m);
                ++v;
                wg.done();
            });
        }
        wg.wait();
        EXPECT_EQ(v, 16);
        v = 0;
    }

    DEF_case(event) {
        {
            co::event ev;
            co::wait_group wg(2);
            v = 777;

            go([wg, ev, &v]() {
                co::atomic_store(&v, 0);
                ev.wait();
                if (v == 1) v = 2;
                wg.done();
            });

            go([wg, ev, &v]() {
                while (v != 0) co::sleep(1);
                v = 1;
                ev.notify_one();
                wg.done();
            });

            wg.wait();
            EXPECT_EQ(v, 2);

            EXPECT_EQ(ev.wait(0), false);
            EXPECT_EQ(ev.wait(0), false);
            ev.notify_one();
            EXPECT_EQ(ev.wait(0), true);
            EXPECT_EQ(ev.wait(0), false);
            EXPECT_EQ(ev.wait(0), false);

            v = 0;
            wg.add(8);
            for (int i = 0; i < 7; ++i) {
                go([wg, ev, &v]() {
                    co::atomic_inc(&v);
                    ev.wait();
                    co::atomic_dec(&v);
                    wg.done();
                });
            }
            std::thread([wg, ev, &v]() {
                co::atomic_inc(&v);
                ev.wait();
                co::atomic_dec(&v);
                wg.done();
            }).detach();

            while (v != 8) co::sleep(1);
            co::sleep(1);
            ev.notify_all();
            wg.wait();
            EXPECT_EQ(v, 0);
            EXPECT_EQ(ev.wait(0), false);

            wg.add(2);
            go([wg, ev, &v]() {
                co::atomic_inc(&v);
                while (v < 2) co::sleep(1);
                ev.wait(1);
                co::atomic_inc(&v);
                wg.done();
            });
            std::thread([wg, ev, &v]() {
                co::atomic_inc(&v);
                while (v < 2) co::sleep(1);
                ev.wait(1);
                co::atomic_inc(&v);
                wg.done();
            }).detach();

            while (v < 4) co::sleep(1);
            ev.notify_all();
            wg.wait();
            EXPECT_EQ(v, 4);
            EXPECT_EQ(ev.wait(0), true);
            EXPECT_EQ(ev.wait(0), false);
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

            EXPECT_EQ(ev.wait(0), false);
            EXPECT_EQ(ev.wait(0), false);
            ev.notify_one();
            EXPECT_EQ(ev.wait(0), true);
            EXPECT_EQ(ev.wait(0), true);

            ev.reset();
            EXPECT_EQ(ev.wait(0), false);
            EXPECT_EQ(ev.wait(0), false);
        }
    }

    DEF_case(pool) {
        co::pool pool(
            []() { return (void*) co::_new<int>(0); },
            [](void* p) { co::_delete((int*)p); },
            8192
        );

        co::wait_group wg;
        wg.add(1);
        go([pool, wg]() {
            {
                co::pool_guard<int> p(pool);
                g_x = *p; // 0
                ++*p;     // -> 1
            }
            {
                co::pool_guard<int> p(pool);
                g_y = *p; // 1
            }
            wg.done();
        });

        wg.wait();
        EXPECT_EQ(g_x, 0);
        EXPECT_EQ(g_y, 1);
    }
}

} // test
