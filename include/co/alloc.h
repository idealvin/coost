#pragma once

#include "def.h"
#include <new>
#include <utility>

namespace co {

// alloc static memory
__coapi void* alloc_static(size_t size);

template <typename T, typename... Args>
inline T* new_static(Args&&... args) {
    return new (alloc_static(sizeof(T))) T(std::forward<Args>(args)...);
}

} // co
