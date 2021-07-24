#pragma once

#include "event.h"
#include "../atomic.h"
#include <utility>

namespace co {

class WaitGroup {
  public:
    WaitGroup(): _n(0) {}
    ~WaitGroup() = default;

    WaitGroup(WaitGroup&& wg) : _ev(std::move(wg._ev)), _n(wg._n) {}

    // increase WaitGroup counter by n (1 by default)
    void add(uint32 n=1) {
        atomic_add(&_n, n);
    }

    // decrease WaitGroup counter by 1 (add() MUST have been called() before calling done())
    void done() {
        if (atomic_dec(&_n) == 0) _ev.signal();
    }

    // blocks until the counter becomes 0
    void wait() { _ev.wait(); }

  private:
    co::Event _ev;
    uint32 _n;
    DISALLOW_COPY_AND_ASSIGN(WaitGroup);
};

} // co
