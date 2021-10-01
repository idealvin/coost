#pragma once

#include "../def.h"
#include "../atomic.h"

namespace co {

class __codec WaitGroup {
  public:
    WaitGroup();
    ~WaitGroup();

    WaitGroup(WaitGroup&& wg) : _p(wg._p) {
        wg._p = 0;
    }

    // copy constructor, allow WaitGroup to be captured by value in lambda.
    WaitGroup(const WaitGroup& wg) : _p(wg._p) {
        atomic_inc(_p);
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
