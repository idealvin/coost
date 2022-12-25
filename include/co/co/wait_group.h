#pragma once

#include "../def.h"
#include "../atomic.h"

namespace co {

class __coapi wait_group {
  public:
    // initialize the counter as @n
    explicit wait_group(uint32 n);

    // the counter is 0 by default
    wait_group() : wait_group(0) {}

    ~wait_group();

    wait_group(wait_group&& wg) noexcept : _p(wg._p) {
        wg._p = 0;
    }

    // copy constructor, just increment the reference count
    wait_group(const wait_group& wg) : _p(wg._p) {
        atomic_inc(_p, mo_relaxed);
    }

    void operator=(const wait_group&) = delete;

    // increase the counter by n (1 by default)
    void add(uint32 n=1) const;

    // decrease the counter by 1
    void done() const;

    // blocks until the counter becomes 0
    void wait() const;

  private:
    uint32* _p;
};

typedef wait_group WaitGroup;

} // co
