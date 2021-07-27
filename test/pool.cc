#include "co/co.h"

class S {
  public:
    S() { _v = this->get_id(); }
    ~S() = default;

    void run() {
        LOG << "S: " << _v;
    }

  private:
    int _v;

    int get_id() {
        static int kId = 0;
        return atomic_inc(&kId);
    }
};

// use DEF_main to make code in main() also run in coroutine.
DEF_main(argc, argv) {
    FLG_cout = true;
    co::Pool p(
        []() { return (void*) new S; },  // ccb
        [](void* p) { delete (S*)p; },   // dcb
        1024                             // max capacity
    );

    co::WaitGroup wg;

    do {
        LOG << "test pop/push begin: ";
        wg.add(8);
        for (int i = 0; i < 8; ++i) {
            LOG << "go: " << i;
            go([p, wg]() { /* capture p and wg by value here, as they are on stack */
                S* s = (S*)p.pop();
                s->run();
                p.push(s);
                LOG << "size: " << p.size();
                wg.done();
            });
        }
        wg.wait();
        LOG << "test pop/push end.. \n";
    } while (0);

    do {
        LOG << "test PoolGuard begin: ";
        wg.add(8);
        for (int i = 0; i < 8; ++i) {
            go([p, wg]() { /* capture p and wg by value here, as they are on stack */
                {
                    co::PoolGuard<S> s(p);
                    s->run();
                }
                LOG << "size: " << p.size();
                wg.done();
            });
        }
        wg.wait();
        LOG << "test PoolGuard end.. \n";
    } while (0);

    p.clear();
    LOG << "size: " << p.size();
    return 0;
}
