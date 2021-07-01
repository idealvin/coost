#include "co/unitest.h"
#include "co/co.h"
#include "co/time.h"

namespace test {

DEF_test(co) {
    DEF_case(sched.SchedManager) {
        int n = (int) FLG_co_sched_num;
        co::xx::Scheduler* s;
        for (int i = 0; i < n; ++i) {
            s = co::xx::scheduler_manager()->next_scheduler();
            EXPECT_EQ(s->id(), i);
        }

        s = co::xx::scheduler_manager()->next_scheduler();
        EXPECT_EQ(s->id(), 0);

        auto& v = co::all_schedulers();
        for (size_t i = 0; i < v.size(); ++i) {
            EXPECT_EQ(v[i]->id(), (int)i);
        }
    }

    DEF_case(sched.Copool) {
        co::xx::Copool pool(4);
        co::xx::Coroutine* c = pool.pop();
        co::xx::Coroutine* d = pool.pop();
        EXPECT_EQ(c->id, 0);
        EXPECT_EQ(d->id, 1);

        pool.push(d);
        co::xx::Coroutine* e = pool.pop();
        EXPECT_EQ(e->id, 1);

        d = pool.pop();
        EXPECT_EQ(d->id, 2);

        EXPECT_EQ(d, pool[2]);
        EXPECT_EQ(e, pool[1]);
        EXPECT_EQ(c, pool[0]);

        co::xx::Coroutine* x = pool.pop();
        co::xx::Coroutine* y = pool.pop();
        co::xx::Coroutine* z = pool.pop();

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
        co::xx::TaskManager mgr;
        mgr.add_new_task((Closure*)8);
        mgr.add_new_task((Closure*)16);
        mgr.add_ready_task((co::xx::Coroutine*)24);
        mgr.add_ready_task((co::xx::Coroutine*)32);

        std::vector<Closure*> cbs;
        std::vector<co::xx::Coroutine*> cos;
        mgr.get_all_tasks(cbs, cos);

        EXPECT_EQ(cbs.size(), 2);
        EXPECT_EQ(cos.size(), 2);
        EXPECT_EQ(cbs[0], (Closure*)8);
        EXPECT_EQ(cbs[1], (Closure*)16);
        EXPECT_EQ(cos[0], (co::xx::Coroutine*)24);
        EXPECT_EQ(cos[1], (co::xx::Coroutine*)32);
    }

    DEF_case(sched.TimerManager) {
        co::xx::TimerManager mgr;
        std::vector<co::xx::Coroutine*> timeout;
        uint32 t = mgr.check_timeout(timeout);
        EXPECT_EQ(timeout.size(), 0);
        EXPECT_EQ(t, -1);

        std::vector<co::xx::Coroutine*> cos;
        for (int i = 0; i < 8; ++i) {
            cos.push_back(new co::xx::Coroutine(i));
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

        cos[2]->state = co::xx::S_ready;

        Timer timer;
        sleep::ms(16);
        auto e = timer.ms();
        t = mgr.check_timeout(timeout);
        EXPECT_GE(timeout.size(), 3);
        if (timeout.size() == 3) {
            EXPECT_LE(t, 50);
            EXPECT_EQ(mgr.assert_it(x), true);
            EXPECT_EQ(mgr.assert_empty(), false);
            mgr.del_timer(x);
            EXPECT_EQ(mgr.assert_it(y), true);
            mgr.del_timer(y);
            EXPECT_EQ(mgr.assert_empty(), true);
        } else if (timeout.size() == 5) {
            EXPECT_EQ(mgr.assert_empty(), true);
        }

        for (int i = 0; i < 8; ++i) {
            delete cos[i];
        }
    }

    //DEF_case(epoll) {}
}

} // namespace test
