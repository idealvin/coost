#pragma once

#include "def.h"
#include <stdlib.h>
#include <new>
#include <utility>

namespace co {

// alloc static memory, do not free or realloc it
__coapi void* alloc_static(size_t size);

// alloc memory and construct a static object on it
template <typename T, typename... Args>
inline T* new_static(Args&&... args) {
    return new (alloc_static(sizeof(T))) T(std::forward<Args>(args)...);
}

// alloc memory for fixed-size objects, do not realloc it
__coapi void* alloc_fixed(size_t size);

// free the memory allocated by alloc_fixed()
__coapi void free_fixed(void* p, size_t size);

// alloc memory and construct an object on it
template <typename T, typename... Args>
inline T* new_fixed(Args&&... args) {
    return new (alloc_fixed(sizeof(T))) T(std::forward<Args>(args)...);
}

// destruct the object created by new_fixed(), and free the memory
template <typename T>
inline void delete_fixed(T* p) {
    if (p) p->~T();
    co::free_fixed((void*)p, sizeof(T));
}

// alloc reallocable memory
__coapi void* alloc(size_t size);

// free the memory allocated by co::alloc() or co::realloc()
__coapi void free(void* p, size_t size);

// realloc the memory allocated by co::alloc() or co::realloc()
//   - if p is NULL, it is equal to co::alloc(new_size)
//   - @new_size must be greater than @old_size
__coapi void* realloc(void* p, size_t old_size, size_t new_size);

struct system_allocator {
    static void* alloc(size_t size) {
        return ::malloc(size);
    }

    static void free(void* p, size_t) {
        return ::free(p);
    }

    static void* realloc(void* p, size_t, size_t new_size) {
        return ::realloc(p, new_size);
    }
};

struct static_allocator {
    static void* alloc(size_t size) {
        return co::alloc_static(size);
    }

    // we do not need free & realloc for static allocator
    static void free(void*, size_t) {}
    static void* realloc(void*, size_t, size_t) { return 0; }
};

struct fixed_allocator {
    static void* alloc(size_t size) {
        return co::alloc_fixed(size);
    }

    static void free(void* p, size_t size) {
        return co::free_fixed(p, size);
    }

    // we do not need realloc for fixed-size allocator
    static void* realloc(void*, size_t, size_t) { return 0; }
};

struct default_allocator {
    static void* alloc(size_t size) {
        return co::alloc(size);
    }

    static void free(void* p, size_t size) {
        return co::free(p, size);
    }

    static void* realloc(void* p, size_t old_size, size_t new_size) {
        return co::realloc(p, old_size, new_size);
    }
};

} // co
