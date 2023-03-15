#include "co/unitest.h"
#include "co/co/thread.h"

namespace test {

DEF_test(thread) {
    DEF_case(sync_event) {
        co::sync_event ev;
        EXPECT_EQ(ev.wait(0), false);
        ev.signal();
        EXPECT_EQ(ev.wait(0), true);
        EXPECT_EQ(ev.wait(0), false);

        co::sync_event em(true, false); // manual-reset
        EXPECT_EQ(em.wait(0), false);
        em.signal();
        EXPECT_EQ(em.wait(0), true);
        em.reset();
        EXPECT_EQ(em.wait(1), false);
    }

    DEF_case(gettid) {
        EXPECT_NE(co::thread_id(), -1);
    }

    DEF_case(thread_ptr) {
        co::thread_ptr<int> pi;
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
