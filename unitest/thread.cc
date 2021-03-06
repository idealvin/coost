#include "co/unitest.h"
#include "co/thread.h"

namespace test {

#ifdef _WIN32
#define MUTEX_REENTRANT true
#else
#define MUTEX_REENTRANT false
#endif

void thread_run(int* x, SyncEvent* ev) {
    *x += 1;
    if (ev != NULL) ev->signal();
}

DEF_test(thread) {
    DEF_case(sync_event) {
        SyncEvent ev;
        EXPECT_EQ(ev.wait(0), false);
        ev.signal();
        EXPECT_EQ(ev.wait(0), true);
        EXPECT_EQ(ev.wait(0), false);

        SyncEvent em(true, false); // manual-reset
        EXPECT_EQ(em.wait(0), false);
        em.signal();
        EXPECT_EQ(em.wait(0), true);
        em.reset();
        EXPECT_EQ(em.wait(1), false);
    }

    DEF_case(thread) {
        int v = 0;
        SyncEvent ev;
        Thread(std::bind(thread_run, &v, &ev)).detach();
        ev.wait();
        EXPECT_EQ(v, 1);
    }

    DEF_case(mutex) {
        Mutex mtx;
        {
            MutexGuard g(mtx);
            EXPECT_EQ(mtx.try_lock(), MUTEX_REENTRANT);
        }

        EXPECT_EQ(mtx.try_lock(), true);
        EXPECT_EQ(mtx.try_lock(), MUTEX_REENTRANT);
        mtx.unlock();
    }

    DEF_case(gettid) {
        EXPECT_NE(current_thread_id(), -1);
    }

    DEF_case(thread_ptr) {
        thread_ptr<int> pi;
        EXPECT(!pi);
        EXPECT(pi == NULL);

        pi.reset(new int(7));
        EXPECT_EQ(*pi, 7);
        EXPECT_EQ(*pi.get(), 7);

        pi = new int(3);
        EXPECT_EQ(*pi, 3);
        pi.reset();
    }
}

} // namespace test
