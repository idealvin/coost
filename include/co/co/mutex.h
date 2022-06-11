#pragma once

#include "../def.h"
#include "../atomic.h"

namespace co {

/**
 * co::Mutex is a mutex lock for coroutines
 *   - It is similar to Mutex for threads.
 *   - Users SHOULD use co::Mutex in coroutine environments only.
 */
class __coapi Mutex {
  public:
    Mutex();
    ~Mutex();

    Mutex(Mutex&& m) : _p(m._p) { m._p = 0; }

    Mutex(const Mutex& m) : _p(m._p) {
        atomic_inc(_p, mo_relaxed);
    }

    void operator=(const Mutex&) = delete;

    /**
     * acquire the lock
     *   - It MUST be called in a coroutine.
     *   - It will block until the lock was acquired by the calling coroutine.
     */
    void lock() const;

    /**
     * release the lock
     *   - It SHOULD be called in the coroutine that holds the lock.
     */
    void unlock() const;

    /**
     * try to acquire the lock
     *   - It SHOULD be called in a coroutine.
     *   - If no coroutine holds the lock, the calling coroutine will get the lock.
     *
     * @return  true if the lock was acquired by the calling coroutine, otherwise false
     */
    bool try_lock() const;

  private:
    uint32* _p;
};

/**
 * guard to release the mutex lock
 *   - lock() is called in the constructor.
 *   - unlock() is called in the destructor.
 */
class __coapi MutexGuard {
  public:
    explicit MutexGuard(const co::Mutex& lock) : _lock(lock) {
        _lock.lock();
    }

    explicit MutexGuard(const co::Mutex* lock) : _lock(*lock) {
        _lock.lock();
    }

    ~MutexGuard() {
        _lock.unlock();
    }

  private:
    const co::Mutex& _lock;
    DISALLOW_COPY_AND_ASSIGN(MutexGuard);
};

} // co
