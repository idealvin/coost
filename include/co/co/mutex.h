#pragma once

#include "../def.h"

namespace co {

// Mutex lock for coroutines, can be also used in non-coroutines since v3.0.1.
class __coapi mutex {
  public:
    mutex();
    ~mutex();

    mutex(mutex&& m) noexcept : _p(m._p) { m._p = 0; }

    // copy constructor, just increment the reference count
    mutex(const mutex& m);

    void operator=(const mutex&) = delete;

    void lock() const;

    void unlock() const;

    bool try_lock() const;

  private:
    void* _p;
};

class __coapi mutex_guard {
  public:
    explicit mutex_guard(const co::mutex& m) : _m(m) {
        _m.lock();
    }

    explicit mutex_guard(const co::mutex* m) : _m(*m) {
        _m.lock();
    }

    ~mutex_guard() {
        _m.unlock();
    }

  private:
    const co::mutex& _m;
    DISALLOW_COPY_AND_ASSIGN(mutex_guard);
};

typedef mutex Mutex;
typedef mutex_guard MutexGuard;

} // co
