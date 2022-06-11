#pragma once

#include "def.h"
#include "atomic.h"
#include <assert.h>
#include <cstddef>
#include <stdlib.h>
#include <new>
#include <utility>
#include <type_traits>

namespace co {

// alloc @size bytes
__coapi void* alloc(size_t size);

// alloc @size bytes, and zero-clear the memory
__coapi void* zalloc(size_t size);

// free the memory
//   - @size: size of the memory
__coapi void free(void* p, size_t size);

// realloc the memory allocated by co::alloc() or co::realloc()
//   - if p is NULL, it is equal to co::alloc(new_size)
//   - @new_size must be greater than @old_size
__coapi void* realloc(void* p, size_t old_size, size_t new_size);


// alloc static memory, do not free or realloc it
__coapi void* static_alloc(size_t size);

// alloc static memory and construct an object on it, do not delete it
//   new T(args)  -->  co::static_new<T>(args)
template <typename T, typename... Args>
inline T* static_new(Args&&... args) {
    return new (co::static_alloc(sizeof(T))) T(std::forward<Args>(args)...);
}


// alloc memory and construct an object on it
//   new T(args)  -->  co::make<T>(args)
template <typename T, typename... Args>
inline T* make(Args&&... args) {
    return new (co::alloc(sizeof(T))) T(std::forward<Args>(args)...);
}

// delete the object created by co::make()
//   delete (T*)p  -->  co::del((T*)p)
template <typename T>
inline void del(T* p, size_t n) {
    if (p) { p->~T(); co::free((void*)p, n); }
}

template <typename T>
inline void del(T* p) {
    co::del(p, sizeof(T));
}


struct system_allocator {
    static void* alloc(size_t n) {
        return ::malloc(n);
    }

    static void free(void* p, size_t) {
        return ::free(p);
    }

    static void* realloc(void* p, size_t, size_t n) {
        return ::realloc(p, n);
    }
};

struct default_allocator {
    static void* alloc(size_t n) {
        return co::alloc(n);
    }

    static void free(void* p, size_t n) {
        return co::free(p, n);
    }

    static void* realloc(void* p, size_t o, size_t n) {
        return co::realloc(p, o, n);
    }
};

// allocator for STL, alternative to std::allocator
template <class T>
struct stl_allocator {
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    typedef value_type* pointer;
    typedef value_type const* const_pointer;
    typedef value_type& reference;
    typedef value_type const& const_reference;

    stl_allocator() noexcept = default;
    stl_allocator(const stl_allocator&) noexcept = default;
    template<class U> stl_allocator(const stl_allocator<U>&) noexcept {}

  #if (__cplusplus >= 201703L)  // C++17
    T* allocate(size_type n) {
        return static_cast<T*>(co::alloc(n * sizeof(T)));
    }
    T* allocate(size_type n, const void*) { return allocate(n); }
  #else
    pointer allocate(size_type n, const void* = 0) {
        return static_cast<pointer>(co::alloc(n * sizeof(value_type)));
    }
  #endif

    void deallocate(T* p, size_type n) { co::free(p, n * sizeof(T)); }

    template <class U, class ...Args>
    void construct(U* p, Args&& ...args) {
        ::new(p) U(std::forward<Args>(args)...);
    }

    template <class U>
    void destroy(U* p) noexcept { p->~U(); }

    template <class U> struct rebind { using other = stl_allocator<U>; };
    pointer address(reference x) const noexcept { return &x; }
    const_pointer address(const_reference x) const noexcept { return &x; }

    size_type max_size() const noexcept {
        return static_cast<size_t>(-1) / sizeof(value_type);
    }
};

template<class T1, class T2>
inline constexpr bool operator==(const stl_allocator<T1>&, const stl_allocator<T2>&) noexcept {
    return true;
}

template<class T1, class T2>
inline constexpr bool operator!=(const stl_allocator<T1>&, const stl_allocator<T2>&) noexcept {
    return false;
}

// manage pointer created by co::make()
//   - co::unique_ptr<int> x(co::make<int>(7));
template<typename T>
class unique_ptr {
  public:
    unique_ptr() noexcept : _p(0) {}
    unique_ptr(std::nullptr_t) noexcept : _p(0) {}
    explicit unique_ptr(T* p) noexcept : _p(p) {}

    unique_ptr(unique_ptr&& x) noexcept : _p(x._p) {
        x._p = 0;
    }

    ~unique_ptr() {
        static_cast<void>(sizeof(T));
        co::del(_p);
    }

    unique_ptr& operator=(unique_ptr&& x) noexcept {
        if (&x != this) {
            this->reset(x._p);
            x._p = 0;
        }
        return *this;
    }

    unique_ptr& operator=(std::nullptr_t) noexcept {
        this->reset();
        return *this;
    }

    T* get() const noexcept { return _p; }

    T* release() noexcept {
        T* p = _p;
        _p = 0;
        return p;
    }

    void swap(unique_ptr& x) noexcept {
        T* p = _p;
        _p = x._p;
        x._p = p;
    }

    void swap(unique_ptr&& x) noexcept {
        x.swap(*this);
    }

    void reset(T* p = 0) noexcept {
        if (_p != p) {
            static_cast<void>(sizeof(T));
            co::del(_p);
            _p = p;
        }
    }

    T* operator->() const {
        assert(_p != 0);
        return _p;
    }

    T& operator*() const {
        assert(_p != 0);
        return *_p;
    }

    explicit operator bool() const noexcept { return _p != 0; }
    bool operator==(T* p) const noexcept { return _p == p; }
    bool operator!=(T* p) const noexcept { return _p != p; }

  private:
    T* _p;
    DISALLOW_COPY_AND_ASSIGN(unique_ptr);
};

// manage shared pointer created by co::make()
//   - co::shared_ptr<int> x(co::make<int>(7));
template<typename T>
class shared_ptr {
  public:
    struct _X {
        _X() : p(0), refn(1) {}
        explicit _X(T* p) : p(p), refn(1) {}
        T* p;
        size_t refn;
    };

    shared_ptr() : _x(0) {}
    shared_ptr(std::nullptr_t) : shared_ptr() {}
    explicit shared_ptr(T* p) : _x(co::make<_X>(p)) { assert(_x); }

    shared_ptr(const shared_ptr& x) noexcept {
        _x = x._x;
        if (_x) atomic_inc(&_x->refn, mo_relaxed);
    }

    shared_ptr(shared_ptr&& x) noexcept {
        _x = x._x;
        x._x = 0;
    }

    ~shared_ptr() {
        if (_x && atomic_dec(&_x->refn, mo_acq_rel) == 0) {
            co::del(_x->p);
            co::del(_x);
        } 
    }

    shared_ptr& operator=(const shared_ptr& o) {
        if (&o != this) shared_ptr<T>(o).swap(*this);
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& o) {
        if (&o != this) shared_ptr<T>(std::move(o)).swap(*this);
        return *this;
    }

    T* get() const noexcept { return _x ? _x->p : 0; }

    void swap(shared_ptr& o) noexcept {
        auto x = _x;
        _x = o._x;
        o._x = x;
    }

    void swap(shared_ptr&& o) noexcept {
        o.swap(*this);
    }

    void reset() {
        if (_x && atomic_dec(&_x->refn, mo_acq_rel) == 0) {
            co::del(_x->p);
            co::del(_x);
        } 
        _x = NULL;
    }

    void reset(T* p) {
        shared_ptr<T>(p).swap(*this);
    }

    size_t use_count() const noexcept {
        return _x ? atomic_load(&_x->refn, mo_relaxed) : 0;
    }

    T* operator->() const {
        assert(_x && _x->p);
        return _x->p;
    }

    T& operator*() const {
        assert(_x && _x->p);
        return *(_x->p);
    }

    explicit operator bool() const noexcept { return this->get() != 0; }
    bool operator==(T* p) const noexcept { return this->get() == p; }
    bool operator!=(T* p) const noexcept { return this->get() != p; }

  private:
    _X* _x;
};

} // co
