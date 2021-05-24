#pragma once

#include <functional>

namespace co {

/**
 * a general pool for coroutine programming 
 *   - Pool is designed to be coroutine-safe, the user does not need to lock it. 
 *   - It stores void* pointers internally and it does not care about the actual 
 *     type of the pointer. 
 *   - It is usually used as a connection pool in network programming. 
 */
class Pool {
  public:
    /**
     * the default constructor 
     *   - In this case, ccb and dcb will be NULL. 
     */
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
    Pool(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap=(size_t)-1);

    Pool(Pool&& p) : _p(p._p) { p._p = 0; }

    Pool(const Pool&) = delete;
    void operator=(const Pool&) = delete;

    /**
     * pop an element from the pool 
     *   - It MUST be called in a coroutine. 
     *   - If the pool is not empty, the element at the back will be popped, otherwise 
     *     ccb is used to create a new element if ccb is not NULL. 
     * 
     * @return  a pointer to an element, or NULL if the pool is empty and the ccb is NULL.
     */
    void* pop();

    /**
     * push an element to the pool 
     *   - It MUST be called in a coroutine. 
     * 
     * @param e  a pointer to an element, it will be ignored if it is NULL.
     */
    void push(void* e);

    /**
     * return size of the internal pool for the current thread.
     */
    size_t size() const;

  private:
    void* _p;
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
template<typename T>
class PoolGuard {
  public:
    explicit PoolGuard(Pool& pool) : _pool(pool) {
        _p = (T*) _pool.pop();
    }

    explicit PoolGuard(Pool* pool) : _pool(*pool) {
        _p = (T*) _pool.pop();
    }

    PoolGuard(const PoolGuard&) = delete;
    void operator=(const PoolGuard&) = delete;

    ~PoolGuard() {
        _pool.push(_p);
    }

    /**
     * overload operator-> for PoolGuard 
     * 
     * @return  a pointer to an object of class T.
     */
    T* operator->() const { assert(_p); return _p; }
    
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
        if (_p != p) { delete _p; _p = p; }
    }

    /**
     * assign a new pointer to PoolGuard 
     * 
     * @param p  a pointer to an object of class T, it MUST be created with operator new. 
     */
    void operator=(T* p) { this->reset(p); }

  private:
    Pool& _pool;
    T* _p;
};

} // co
