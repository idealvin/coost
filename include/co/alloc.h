#pragma once

#include "def.h"
#include <stdlib.h>
#include <assert.h>
#include <cstddef>
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


// alloc memory for fixed-size objects, do not realloc it
__coapi void* fixed_alloc(size_t size);

// alloc memory and construct an object on it
//   new T(args)  -->  co::make<T>(args)
template <typename T, typename... Args>
inline T* make(Args&&... args) {
    return new (co::fixed_alloc(sizeof(T))) T(std::forward<Args>(args)...);
}

// delete the object created by co::make()
//   delete (T*)p  -->  co::del((T*)p)
template <typename T>
inline void del(T* p) {
    if (p) { p->~T(); co::free((void*)p, sizeof(T)); }
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

struct static_allocator {
    static void* alloc(size_t n) {
        return co::static_alloc(n);
    }

    // we do not need free & realloc for static allocator
    static void free(void*, size_t) {}
    static void* realloc(void*, size_t, size_t) { return 0; }
};

struct fixed_allocator {
    static void* alloc(size_t size) {
        return co::fixed_alloc(size);
    }

    static void free(void* p, size_t size) {
        return co::free(p, size);
    }

    // we do not need realloc for fixed-size allocator
    static void* realloc(void*, size_t, size_t) { return 0; }
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
        return static_cast<T*>(co::fixed_alloc(n * sizeof(T)));
    }
    T* allocate(size_type n, const void*) { return allocate(n); }
#else
    pointer allocate(size_type n, const void* = 0) {
        return static_cast<pointer>(co::fixed_alloc(n * sizeof(value_type)));
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

} // co
