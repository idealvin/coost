#include "co/unitest.h"
#include "co/atomic.h"
#include "co/thread.h"
#include "co/time.h"

namespace test {

int g_c = 0;
int g_v = 0;
int g_t = 0;

DEF_test(thread) {
    DEF_case(sync_event) {
        co::sync_event ev;
        EXPECT_EQ(ev.wait(0), false);
        ev.notify_one();
        EXPECT_EQ(ev.wait(0), true);
        EXPECT_EQ(ev.wait(0), false);

        co::sync_event xv;
        auto f = [&ev, &xv]() {
            co::atomic_inc(&g_t, co::mo_relaxed);
            ev.wait();
            auto s = co::atomic_inc(&g_c, co::mo_relaxed);
            co::atomic_store(&g_v, s, co::mo_release);
            xv.notify_one();
        };

        std::thread(f).detach();
        std::thread(f).detach();

        while (co::atomic_load(&g_t, co::mo_relaxed) != 2) {
            time::sleep(1);
        }
        ev.notify_one();
        xv.wait();
        EXPECT(!xv.wait(1));
        EXPECT_EQ(co::atomic_load(&g_v, co::mo_acquire), 1);

        EXPECT(!ev.wait(0));
        ev.notify_all();
        xv.wait();
        EXPECT(!xv.wait(0));
        EXPECT_EQ(co::atomic_load(&g_v, co::mo_acquire), 2);

        co::sync_event em(true, false); // manual-reset
        EXPECT_EQ(em.wait(0), false);
        em.notify_one();
        EXPECT_EQ(em.wait(0), true);
        em.reset();
        EXPECT_EQ(em.wait(1), false);

        co::atomic_store(&g_c, 0, co::mo_relaxed);
        co::atomic_store(&g_t, 0, co::mo_relaxed);
        co::atomic_store(&g_v, 0, co::mo_relaxed);

        auto g = [&em, &xv]() {
            co::atomic_inc(&g_t, co::mo_relaxed);
            em.wait();
            auto s = co::atomic_inc(&g_c, co::mo_relaxed);
            co::atomic_store(&g_v, s, co::mo_release);
            xv.notify_one();
        };

        std::thread(g).detach();
        while (co::atomic_load(&g_t, co::mo_relaxed) != 1) {
            time::sleep(1);
        }
        em.notify_one();
        xv.wait();
        EXPECT(!xv.wait(0));
        EXPECT(em.wait(0));
        EXPECT_EQ(co::atomic_load(&g_v, co::mo_acquire), 1);

        std::thread(g).detach();
        while (co::atomic_load(&g_t, co::mo_relaxed) != 2) {
            time::sleep(1);
        }

        xv.wait();
        EXPECT(!xv.wait(0));
        EXPECT_EQ(co::atomic_load(&g_v, co::mo_acquire), 2);

        EXPECT(em.wait(0));
        em.reset();
        EXPECT(!em.wait(0));
    }

    DEF_case(thread_id) {
        EXPECT_NE(co::thread_id(), -1);
    }
}

} // namespace test
