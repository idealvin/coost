#pragma once

#include "../def.h"
#include "../atomic.h"

namespace co {

class __coapi WaitGroup {
  public:
    // initialize the counter as @n
    explicit WaitGroup(uint32 n);

    // the counter is 0 by default
    WaitGroup() : WaitGroup(0) {}

    ~WaitGroup();

    WaitGroup(WaitGroup&& wg) : _p(wg._p) {
        wg._p = 0;
    }

    // copy constructor, allow WaitGroup to be captured by value in lambda.
    WaitGroup(const WaitGroup& wg) : _p(wg._p) {
        atomic_inc(_p, mo_relaxed);
    }

    void operator=(const WaitGroup&) = delete;

    // increase WaitGroup counter by n (1 by default)
    void add(uint32 n=1) const;

    // decrease WaitGroup counter by 1
    void done() const;

    // blocks until the counter becomes 0
    void wait() const;

  private:
    uint32* _p;
};

} // co
