#pragma once

#include "def.h"
#include "assert.h"
#include "atomic.h"
#include <string.h>
#include <new>
#include <utility>
#include <type_traits>

namespace co {
namespace xx {

struct MemInit {
    MemInit();
    ~MemInit();
};

static MemInit g_mem_init;

// DO NOT modify this value!
constexpr size_t g_max_align = 256;

} // xx

void* alloc(size_t n);

// @align: must be power of 2, and its maximum value is 256
void* alloc(size_t n, size_t align);

// free the memory block pointed to by p
//   - @p: pointer to the memory block, may be NULL
//   - @n: size of the block, MUST be the same as the size used in alloc or realloc
void free(void* p, size_t n);

// reallocate the memory block pointed to by p
//   - @p: pointer to the memory block, may be NULL
//   - @o: old size, must be the same as the size used in alloc() or a previous realloc()
//   - @n: new size, must be greater than @o
//   - return: pointer to the reallocated block, may be the same as @p, or NULL on failure
void* realloc(void* p, size_t o, size_t n);

// alloc and zero-clear the memory
void* zalloc(size_t n);

// virtual alloc, the memory is page-aligned and zero-cleared
void* valloc(size_t n);

// virtual free
void vfree(void* p, size_t n);

// use co::free(s, strlen(s) + 1) to free the memory
char* strdup(const char* s);

template<typename T, typename... Args>
inline T* _new(Args&&... args) {
    constexpr size_t A = alignof(T);
    constexpr size_t N = sizeof(T);
    static_assert(A <= xx::g_max_align, "");
    const auto p = A <= 16 ? co::alloc(N) : co::alloc(N, A);
    runtime_assert(p);
    return new (p) T(std::forward<Args>(args)...);
}

template<typename T>
inline void _delete(T* p, size_t n=sizeof(T)) {
    if (p) {
        p->~T();
        co::free((void*)p, n);
    }
}

struct _D {
    template<typename T>
    explicit _D(T* o) noexcept : _o((void*)o) {
        _d = [](void* p) {
            static_cast<T*>(p)->~T();
        };
    }

    void operator()() const { _d(_o); }

    void* _o;
    void (*_d)(void*);
};

void* _static_alloc(size_t n, size_t align=sizeof(void*));
void _add_destructor(_D&& d, int x);

template<typename T, int N, typename... Args>
inline T* _smake(Args&&... args) {
    static_assert(alignof(T) <= xx::g_max_align, "");
    T* const p = (T*) _static_alloc(sizeof(T), alignof(T));
    new(p) T(std::forward<Args>(args)...);
    if (!std::is_trivially_destructible_v<T>) _add_destructor(_D(p), N);
    return p;
}

template<typename T, typename... Args>
inline T* _make_rootic(Args&&... args) {
    return _smake<T, 0>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
inline T* _make_static(Args&&... args) {
    return _smake<T, 1>(std::forward<Args>(args)...);
}

// make non-dependent static object at the root level
template<typename T, typename... Args>
inline T* make_rootic(Args&&... args) {
    return _smake<T, 2>(std::forward<Args>(args)...);
}

// make static object, which will be destructed automatically at exit
//   - T* p = co::make_static<T>(args)
template<typename T, typename... Args>
inline T* make_static(Args&&... args) {
    return _smake<T, 3>(std::forward<Args>(args)...);
}

// auto s = co::make_unique<co::string>(32, 'x');
template<typename T>
struct unique {
    constexpr unique() noexcept : _p(nullptr) {}
    constexpr unique(std::nullptr_t) noexcept : _p(nullptr) {}
    unique(unique& x) noexcept : _p(x._p) { x._p = nullptr; }
    unique(unique&& x) noexcept : _p(x._p) { x._p = nullptr; }
    ~unique() { this->reset(); }

    unique(const unique&) = delete;

    unique& operator=(unique&& x) noexcept {
        if (&x != this) {
            this->reset();
            _p = x._p;
            x._p = nullptr;
        }
        return *this;
    }

    unique& operator=(unique& x) noexcept {
        return this->operator=(std::move(x));
    }

    template<typename X, typename = std::enable_if_t<!std::is_same_v<T, X>>>
    unique(unique<X>& x) noexcept {
        static_assert(std::is_base_of_v<T, X>);
        static_assert(std::has_virtual_destructor_v<T>);
        _p = x.get();
        *(void**)&x = nullptr;
    }

    template<typename X, typename = std::enable_if_t<!std::is_same_v<T, X>>>
    unique(unique<X>&& x) noexcept {
        static_assert(std::is_base_of_v<T, X>);
        static_assert(std::has_virtual_destructor_v<T>);
        _p = x.get();
        *(void**)&x = nullptr;
    }

    template<typename X, typename = std::enable_if_t<!std::is_same_v<T, X>>>
    unique& operator=(unique<X>&& x) noexcept {
        static_assert(std::is_base_of_v<T, X>);
        static_assert(std::has_virtual_destructor_v<T>);
        if ((void*)&x != (void*)this) {
            this->reset();
            _p = x.get();
            *(void**)&x = nullptr;
        }
        return *this;
    }

    template<typename X, typename = std::enable_if_t<!std::is_same_v<T, X>>>
    unique& operator=(unique<X>& x) noexcept {
        static_assert(std::is_base_of_v<T, X>);
        static_assert(std::has_virtual_destructor_v<T>);
        return this->operator=(std::move(x));
    }

    T* get() const noexcept { return _p; }
    T* operator->() const noexcept { runtime_assert(_p); return _p; }
    T& operator*() const noexcept { runtime_assert(_p); return *_p; }

    bool operator==(T* p) const noexcept { return _p == p; }
    bool operator!=(T* p) const noexcept { return _p != p; }
    explicit operator bool() const noexcept { return _p != nullptr; }

    void reset() noexcept {
        if (_p) {
            static_cast<void>(sizeof(T));
            _p->~T();
            co::free((char*)_p - _s[-2], _s[-1]);
            _p = nullptr;
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
    union { T* _p; uint32* _s; };
};

template<typename T, typename... Args>
inline unique<T> make_unique(Args&&... args) {
    struct S { uint32 o; uint32 n; T t; };
    static_assert(alignof(S) <= xx::g_max_align, "");

    unique<T> x;
    S* const s = (S*) co::alloc(sizeof(S), alignof(S));
    runtime_assert(s);

    struct Guard {
        void* s;
        ~Guard() { if (s) co::free(s, sizeof(S)); }
    } guard{s};

    uint32* const p = (uint32*) &s->t;
    new (p) T(std::forward<Args>(args)...);
    guard.s = nullptr;

    static_assert(sizeof(S) <= co::max_uint32, "");
    p[-1] = sizeof(S);
    p[-2] = static_cast<uint32>((char*)p - (char*)s);
    *(void**)&x = p;
    return x;
}

// auto s = co::make_shared<co::string>(32, 'x');
template<typename T>
struct shared {
    constexpr shared() noexcept : _p(nullptr) {}
    constexpr shared(std::nullptr_t) noexcept : _p(nullptr) {}

    shared(const shared& x) noexcept {
        _s = x._s;
        if (_s) atomic_inc(&_s[-3], mo_relaxed);
    }

    shared(shared&& x) noexcept {
        _p = x._p;
        x._p = nullptr;
    }

    ~shared() { this->reset(); }

    shared& operator=(const shared& x) noexcept {
        if (&x != this) shared<T>(x).swap(*this);
        return *this;
    }

    shared& operator=(shared&& x) noexcept {
        if (&x != this) shared<T>(std::move(x)).swap(*this);
        return *this;
    }

    template<typename X, typename = std::enable_if_t<!std::is_same_v<T, X>>>
    shared(const shared<X>& x) noexcept {
        static_assert(std::is_base_of_v<T, X>);
        static_assert(std::has_virtual_destructor_v<T>);
        _p = x.get();
        if (_s) atomic_inc(&_s[-3], mo_relaxed);
    }

    template<typename X, typename = std::enable_if_t<!std::is_same_v<T, X>>>
    shared(shared<X>&& x) noexcept {
        static_assert(std::is_base_of_v<T, X>);
        static_assert(std::has_virtual_destructor_v<T>);
        _p = x.get();
        *(void**)&x = nullptr;
    }

    template<typename X, typename = std::enable_if_t<!std::is_same_v<T, X>>>
    shared& operator=(const shared<X>& x) noexcept {
        static_assert(std::is_base_of_v<T, X>);
        static_assert(std::has_virtual_destructor_v<T>);
        if ((void*)&x != (void*)this) shared<T>(x).swap(*this);
        return *this;
    }

    template<typename X, typename = std::enable_if_t<!std::is_same_v<T, X>>>
    shared& operator=(shared<X>&& x) noexcept {
        static_assert(std::is_base_of_v<T, X>);
        static_assert(std::has_virtual_destructor_v<T>);
        if ((void*)&x != (void*)this) shared<T>(std::move(x)).swap(*this);
        return *this;
    }

    T* get() const noexcept { return _p; }
    T* operator->() const noexcept { runtime_assert(_p); return _p; }
    T& operator*() const noexcept { runtime_assert(_p); return *_p; }

    bool operator==(T* p) const noexcept { return _p == p; }
    bool operator!=(T* p) const noexcept { return _p != p; }
    explicit operator bool() const noexcept { return _p != nullptr; }

    void reset() noexcept {
        if (_s) {
            if (atomic_dec(&_s[-3], mo_acq_rel) == 0) {
                static_cast<void>(sizeof(T));
                _p->~T();
                co::free((char*)_p - _s[-2], _s[-1]);
            }
            _p = nullptr;
        }
    }

    size_t ref_count() const noexcept {
        return _s ? atomic_load(&_s[-3], mo_relaxed) : 0;
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
    union { T* _p; uint32* _s; };
};

template<typename T, typename... Args>
inline shared<T> make_shared(Args&&... args) {
    struct S { uint32 r; uint32 o; uint32 n; T t; };
    static_assert(alignof(S) <= xx::g_max_align, "");

    shared<T> x;
    S* const s = (S*) co::alloc(sizeof(S), alignof(S));
    runtime_assert(s);

    struct Guard {
        void* s;
        ~Guard() { if (s) co::free(s, sizeof(S)); }
    } guard{s};

    uint32* const p = (uint32*) &s->t;
    new (p) T(std::forward<Args>(args)...);
    guard.s = nullptr;

    p[-1] = sizeof(S);
    p[-2] = static_cast<uint32>((char*)p - (char*)s);
    p[-3] = 1;
    *(void**)&x = p;
    return x;
}

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

#if (__cplusplus >= 201703L) // C++17
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
constexpr bool operator==(const stl_allocator<T1>&, const stl_allocator<T2>&) noexcept {
    return true;
}

template<class T1, class T2>
constexpr bool operator!=(const stl_allocator<T1>&, const stl_allocator<T2>&) noexcept {
    return false;
}

} // co
