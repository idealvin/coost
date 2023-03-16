#pragma once

#include "def.h"
#include "god.h"
#include "atomic.h"
#include <assert.h>
#include <cstddef>
#include <stdlib.h>
#include <new>
#include <utility>
#include <type_traits>
#include <functional>

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


// alloc memory and construct an object on it
//   - T* p = co::make<T>(args)
template<typename T, typename... Args>
inline T* make(Args&&... args) {
    return new (co::alloc(sizeof(T))) T(std::forward<Args>(args)...);
}

// delete the object created by co::make()
//   - co::del((T*)p)
template<typename T>
inline void del(T* p, size_t n=sizeof(T)) {
    if (p) { p->~T(); co::free((void*)p, n); }
}

// used internally by coost, do not call it
__coapi void* _salloc(size_t n, int x);

// used internally by coost, do not call it
__coapi void _dealloc(std::function<void()>&& f, int x);

// used internally by coost, do not call it
template<typename T, typename... Args>
inline T* _make_static(Args&&... args) {
    static_assert(sizeof(T) <= 4096, "");
    const auto p = _salloc(sizeof(T), 1);
    if (p) {
        new(p) T(std::forward<Args>(args)...);
        const bool x = god::is_trivially_destructible<T>();
        !x ? _dealloc([p](){ ((T*)p)->~T(); }, 1) : (void)0;
    }
    return (T*)p;
}

// create a static object, which will be destructed automatically at exit
//   - T* p = co::make_static<T>(args)
template<typename T, typename... Args>
inline T* make_static(Args&&... args) {
    static_assert(sizeof(T) <= 4096, "");
    const auto p = _salloc(sizeof(T), 0);
    if (p) {
        new(p) T(std::forward<Args>(args)...);
        const bool x = god::is_trivially_destructible<T>();
        !x ? _dealloc([p](){ ((T*)p)->~T(); }, 0) : (void)0;
    }
    return (T*)p;
}

// similar to std::unique_ptr
//   - It is **not allowed** to create unique object from a nake pointer,
//     use **make_unique** instead.
//   - eg.
//     auto s = co::make_unique<fastring>(32, 'x');
template<typename T>
class unique {
  public:
    constexpr unique() noexcept : _p(0) {}
    constexpr unique(std::nullptr_t) noexcept : _p(0) {}
    unique(unique& x) noexcept : _p(x._p) { x._p = 0; }
    unique(unique&& x) noexcept : _p(x._p) { x._p = 0; }
    ~unique() { this->reset(); }

    unique& operator=(unique&& x) {
        if (&x != this) { this->reset(); _p = x._p; x._p = 0; }
        return *this;
    }

    unique& operator=(unique& x) {
        return this->operator=(std::move(x));
    }

    template<typename X, god::if_t<
        god::is_base_of<T, X>() && god::has_virtual_destructor<T>(), int
    > = 0>
    unique(unique<X>& x) noexcept : _p(x.get()) { *(void**)&x = 0; }

    template<typename X, god::if_t<
        god::is_base_of<T, X>() && god::has_virtual_destructor<T>(), int
    > = 0>
    unique(unique<X>&& x) noexcept : _p(x.get()) { *(void**)&x = 0; }

    template<typename X, god::if_t<
        god::is_base_of<T, X>() && god::has_virtual_destructor<T>(), int
    > = 0>
    unique& operator=(unique<X>&& x) {
        if ((void*)&x != (void*)this) {
            this->reset();
            _p = x.get();
            *(void**)&x = 0;
        }
        return *this;
    }

    template<typename X, god::if_t<
        god::is_base_of<T, X>() && god::has_virtual_destructor<T>(), int
    > = 0>
    unique& operator=(unique<X>& x) {
        return this->operator=(std::move(x));
    }

    T* get() const noexcept { return _p; }
    T* operator->() const { assert(_p); return _p; }
    T& operator*() const { assert(_p); return *_p; }

    explicit operator bool() const noexcept { return _p != 0; }
    bool operator==(T* p) const noexcept { return _p == p; }
    bool operator!=(T* p) const noexcept { return _p != p; }

    void reset() {
        if (_p) {
            static_cast<void>(sizeof(T));
            _p->~T();
            co::free(_s - 1, _s[-1]);
            _p = 0;
        }
    }

    void swap(unique& x) noexcept {
        T* const p = _p;
        _p = x._p;
        x._p = p;
    }

    void swap(unique&& x) noexcept {
        x.swap(*this);
    }

  private:
    union {
        T* _p;
        size_t* _s;
    };
};

template<typename T, typename... Args>
inline unique<T> make_unique(Args&&... args) {
    struct S { size_t n; T o; };
    size_t* s = (size_t*) co::alloc(sizeof(S));
    if (s) {
        new(s + 1) T(std::forward<Args>(args)...);
        *s = sizeof(S);
    }
    unique<T> x;
    *(void**)&x = s + 1;
    return x;
}

// similar to std::shared_ptr
//   - It is **not allowed** to create shared object from a nake pointer,
//     use **make_shared** instead.
//   - eg.
//     auto s = co::make_shared<fastring>(32, 'x');
template<typename T>
class shared {
  public:
    constexpr shared() noexcept : _p(0) {}
    constexpr shared(std::nullptr_t) noexcept : _p(0) {}

    shared(const shared& x) noexcept {
        _p = x._p;
        if (_p) atomic_inc(_s - 2, mo_relaxed);
    }

    shared(shared&& x) noexcept {
        _p = x._p;
        x._p = 0;
    }

    ~shared() {
        if (_p && atomic_dec(_s - 2, mo_acq_rel) == 0) {
            static_cast<void>(sizeof(T));
            _p->~T();
            co::free(_s - 2, _s[-1]);
            _p = 0;
        } 
    }

    shared& operator=(const shared& x) {
        if (&x != this) shared<T>(x).swap(*this);
        return *this;
    }

    shared& operator=(shared&& x) {
        if (&x != this) shared<T>(std::move(x)).swap(*this);
        return *this;
    }

    template<typename X, god::if_t<
        god::is_base_of<T, X>() && god::has_virtual_destructor<T>(), int
    > = 0>
    shared(const shared<X>& x) noexcept {
        _p = x.get();
        if (_p) atomic_inc(_s - 2, mo_relaxed);
    }

    template<typename X, god::if_t<
        god::is_base_of<T, X>() && god::has_virtual_destructor<T>(), int
    > = 0>
    shared(shared<X>&& x) noexcept {
        _p = x.get();
        *(void**)&x = 0;
    }

    template<typename X, god::if_t<
        god::is_base_of<T, X>() && god::has_virtual_destructor<T>(), int
    > = 0>
    shared& operator=(const shared<X>& x) {
        if ((void*)&x != (void*)this) shared<T>(x).swap(*this);
        return *this;
    }

    template<typename X, god::if_t<
        god::is_base_of<T, X>() && god::has_virtual_destructor<T>(), int
    > = 0>
    shared& operator=(shared<X>&& x) {
        if ((void*)&x != (void*)this) shared<T>(std::move(x)).swap(*this);
        return *this;
    }

    T* get() const noexcept { return _p; }
    T* operator->() const { assert(_p); return _p; }
    T& operator*() const { assert(_p); return *_p; }

    explicit operator bool() const noexcept { return _p != 0; }
    bool operator==(T* p) const noexcept { return _p == p; }
    bool operator!=(T* p) const noexcept { return _p != p; }

    void reset() {
        if (_p && atomic_dec(_s - 2, mo_acq_rel) == 0) {
            _p->~T();
            co::free(_s - 2, _s[-1]);
        } 
        _p = 0;
    }

    size_t ref_count() const noexcept {
        return _s ? atomic_load(_s - 2, mo_relaxed) : 0;
    }

    size_t use_count() const noexcept {
        return this->ref_count();
    }

    void swap(shared& x) noexcept {
        T* const p = _p;
        _p = x._p;
        x._p = p;
    }

    void swap(shared&& x) noexcept {
        x.swap(*this);
    }

  private:
    union {
        T* _p;
        uint32* _s;
    };
};

template<typename T, typename... Args>
inline shared<T> make_shared(Args&&... args) {
    struct S { uint32 refn; uint32 size; T o; };
    uint32* s = (uint32*) co::alloc(sizeof(S));
    if (s) {
        new(s + 2) T(std::forward<Args>(args)...);
        s[0] = 1;
        s[1] = sizeof(S);
    }
    shared<T> x;
    *(void**)&x = s + 2;
    return x;
}


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

// allocator for STL, alternative to std::allocator
template<class T>
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

    template<class U, class ...Args>
    void construct(U* p, Args&& ...args) {
        ::new(p) U(std::forward<Args>(args)...);
    }

    template<class U>
    void destroy(U* p) noexcept { p->~U(); }

    template<class U> struct rebind { using other = stl_allocator<U>; };
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

} // co
