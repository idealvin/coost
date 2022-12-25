#pragma once

#include "../def.h"
#include "../atomic.h"

namespace co {

/**
 * mutex lock for coroutines
 *   - It can be also used in non-coroutines since v3.0.1.
 */
class __coapi mutex {
  public:
    mutex();
    ~mutex();

    mutex(mutex&& m) noexcept : _p(m._p) { m._p = 0; }

    // copy constructor, just increment the reference count
    mutex(const mutex& m) : _p(m._p) {
        atomic_inc(_p, mo_relaxed);
    }

    void operator=(const mutex&) = delete;

    void lock() const;

    void unlock() const;

    bool try_lock() const;

  private:
    uint32* _p;
};

class __coapi mutex_guard {
  public:
    explicit mutex_guard(const co::mutex& lock) : _lock(lock) {
        _lock.lock();
    }

    explicit mutex_guard(const co::mutex* lock) : _lock(*lock) {
        _lock.lock();
    }

    ~mutex_guard() {
        _lock.unlock();
    }

  private:
    const co::mutex& _lock;
    DISALLOW_COPY_AND_ASSIGN(mutex_guard);
};

typedef mutex Mutex;
typedef mutex_guard MutexGuard;

} // co
