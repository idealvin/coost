#pragma once

#include "../def.h"
#include "../atomic.h"
#include <memory>
#include <functional>

namespace co {

/**
 * a general pool for coroutine programming
 *   - Pool is designed to be coroutine-safe, users do not need to lock it.
 *   - It stores void* pointers internally and it does not care about the actual
 *     type of the pointer.
 *   - It is usually used as a connection pool in network programming.
 * 
 *   - NOTE: Each thread holds its own pool, users SHOULD call pop() and push() 
 *     in the same thread.
 */
class __coapi Pool {
  public:
    // default constructor without ccb and dcb 
    Pool();
    ~Pool();

    /**
     * the constructor with parameters
     *
     * @param ccb  a create callback like:   []() { return (void*) new T; }
     *             it is used to create an element when pop from an empty pool.
     * @param dcb  a destroy callback like:  [](void* p) { delete (T*)p; }
     *             it is used to destroy an element.
     * @param cap  max capacity of the pool for each thread, -1 for unlimited.
     *             this argument is ignored if dcb is NULL.
     *             default: -1.
     */
    Pool(std::function<void* ()>&& ccb, std::function<void(void*)>&& dcb, size_t cap=(size_t)-1);

    Pool(Pool&& p) : _p(p._p) { p._p = 0; }

    Pool(const Pool& p) : _p(p._p) {
        atomic_inc(_p, mo_relaxed);
    }

    void operator=(const Pool&) = delete;

    /**
     * pop an element from the pool of the current thread 
     *   - It MUST be called in a coroutine.
     *   - If the pool is empty and ccb is set, ccb() will be called to create a new element.
     *
     * @return  a pointer to an element, or NULL if pool is empty and ccb is not set.
     */
    void* pop() const;

    /**
     * push an element to the pool of the current thread 
     *   - It MUST be called in a coroutine.
     *   - Users SHOULD call push() and pop() in the same thread.
     *
     * @param e  a pointer to an element, NULL pointers will be ignored.
     */
    void push(void* e) const;

    /**
     * return pool size of the current thread 
     *   - It MUST be called in a coroutine.
     */
    size_t size() const;

    /**
     * clear pools of all threads 
     *   - It can be called from any where. 
     *   - If dcb is set, dcb() will be called to destroy elements in the pools. 
     */
    void clear() const;

  private:
    uint32* _p;
};

/**
 * guard to push an element back to co::Pool
 *   - Pool::pop() is called in the constructor.
 *   - Pool::push() is called in the destructor.
 *   - operator->() is overloaded, so it has a behavior similar to std::unique_ptr.
 *
 *   - usage:
 *     struct T { void hello(); };
 *     co::Pool pool(
 *         []() { return (void*) new T; },  // ccb
 *         [](void* p) { delete (T*)p; }    // dcb
 *     );
 *
 *     co::PoolGuard<T> g(pool);
 *     g->hello();
 */
template<typename T, typename D=std::default_delete<T>>
class PoolGuard {
  public:
    explicit PoolGuard(const Pool& pool) : _pool(pool) {
        _p = (T*)_pool.pop();
    }

    explicit PoolGuard(const Pool* pool) : _pool(*pool) {
        _p = (T*)_pool.pop();
    }

    ~PoolGuard() {
        _pool.push(_p);
    }

    /**
     * overload operator-> for PoolGuard
     *
     * @return  a pointer to an object of class T.
     */
    T* operator->() const { assert(_p); return _p; }
    T& operator*()  const { assert(_p); return *_p; }

    bool operator==(T* p) const { return _p == p; }
    bool operator!=(T* p) const { return _p != p; }
    bool operator!() const { return _p == NULL; }
    explicit operator bool() const { return _p != NULL; }

    /**
     * get the pointer owns by PoolGuard
     *
     * @return  a pointer to an object of class T
     */
    T* get() const { return _p; }

    /**
     * reset the pointer owns by PoolGuard
     *
     * @param p  a pointer to an object of class T, it MUST be created with operator new.
     *           default: NULL.
     */
    void reset(T* p = 0) {
        if (_p != p) { if (_p) D()(_p); _p = p; }
    }

    /**
     * assign a new pointer to PoolGuard
     *
     * @param p  a pointer to an object of class T, it MUST be created with operator new.
     */
    void operator=(T* p) { this->reset(p); }

  private:
    const Pool& _pool;
    T* _p;
    DISALLOW_COPY_AND_ASSIGN(PoolGuard);
};

} // co
