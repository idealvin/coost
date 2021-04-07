#include "co/unitest.h"
#include "co/co.h"

namespace test {

DEF_test(co) {
    EXPECT(co::null_timer_id == co::timer_id_t());

    DEF_case(sched.SchedManager) {
        int n = (int) FLG_co_sched_num;
        co::Scheduler* s;
        for (int i = 0; i < n; ++i) {
            s = co::sched_mgr()->next();
            EXPECT_EQ(s->id(), i);
        }

        s = co::sched_mgr()->next();
        EXPECT_EQ(s->id(), 0);
    }

    DEF_case(sched.Copool) {
        co::Copool pool(4);
        co::Coroutine* c = pool.pop();
        co::Coroutine* d = pool.pop();
        EXPECT_EQ(c->id, 0);
        EXPECT_EQ(d->id, 1);

        pool.push(d);
        co::Coroutine* e = pool.pop();
        EXPECT_EQ(e->id, 1);

        d = pool.pop();
        EXPECT_EQ(d->id, 2);

        EXPECT_EQ(d, pool[2]);
        EXPECT_EQ(e, pool[1]);
        EXPECT_EQ(c, pool[0]);

        co::Coroutine* x = pool.pop();
        co::Coroutine* y = pool.pop();
        co::Coroutine* z = pool.pop();

        EXPECT_EQ(d, pool[2]);
        EXPECT_EQ(e, pool[1]);
        EXPECT_EQ(c, pool[0]);
        EXPECT_EQ(x, pool[3]);
        EXPECT_EQ(y, pool[4]);
        EXPECT_EQ(z, pool[5]);

        pool.push(c);
        pool.push(d);
        pool.push(e);
        pool.push(x);
        pool.push(y);
        pool.push(z);
    }

    DEF_case(sched.TaskManager) {
        co::TaskManager mgr;
        mgr.add_new_task((Closure*)8);
        mgr.add_new_task((Closure*)16);
        mgr.add_ready_task((co::Coroutine*)24);
        mgr.add_ready_task((co::Coroutine*)32);

        std::vector<Closure*> cbs;
        std::vector<co::Coroutine*> cos;
        mgr.get_all_tasks(cbs, cos);

        EXPECT_EQ(cbs.size(), 2);
        EXPECT_EQ(cos.size(), 2);
        EXPECT_EQ(cbs[0], (Closure*)8);
        EXPECT_EQ(cbs[1], (Closure*)16);
        EXPECT_EQ(cos[0], (co::Coroutine*)24);
        EXPECT_EQ(cos[1], (co::Coroutine*)32);
    }

    DEF_case(sched.TimerManager) {
        co::TimerManager mgr;
        std::vector<co::Coroutine*> timeout;
        uint32 t = mgr.check_timeout(timeout);
        EXPECT_EQ(timeout.size(), 0);
        EXPECT_EQ(t, -1);

        std::vector<co::Coroutine*> cos;
        for (int i = 0; i < 8; ++i) {
            cos.push_back(new co::Coroutine(i));
        }

        auto x = mgr.add_timer(64, cos[4]);
        auto y = mgr.add_io_timer(64, cos[5]);

        mgr.add_timer(4, cos[0]);
        mgr.add_timer(4, cos[1]);
        mgr.add_io_timer(4, cos[2]);
        mgr.add_io_timer(4, cos[3]);

        t = mgr.check_timeout(timeout);
        EXPECT_EQ(timeout.size(), 0);
        EXPECT_LE(t, 4);

        cos[2]->state = co::S_ready;
        sleep::ms(16);
        t = mgr.check_timeout(timeout);
        EXPECT_EQ(timeout.size(), 3);
        EXPECT_LE(t, 50);
        EXPECT_EQ(mgr.assert_it(x), true);

        EXPECT_EQ(mgr.assert_empty(), false);
        mgr.del_timer(x);
        EXPECT_EQ(mgr.assert_it(y), true);
        mgr.del_timer(y);
        EXPECT_EQ(mgr.assert_empty(), true);

        for (int i = 0; i < 8; ++i) {
            delete cos[i];
        }
    }

    //DEF_case(epoll) {}
}

} // namespace test
