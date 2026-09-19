#pragma once

#include "mem.h"
#include <utility>
#include <tuple>
#include <type_traits>

namespace co {

// closure called once only
struct once_closure {
    constexpr once_closure() noexcept : _p(0) {}
    once_closure(void (*f)()) noexcept : _p((void*)f) {}

    once_closure(once_closure&& c) noexcept
        : _p(c._p) {
        c._p = 0;
    }

    once_closure(const once_closure&) = delete;
    void operator=(const once_closure&) = delete;
    void operator=(once_closure&&) = delete;

    template<typename F, typename = std::enable_if_t<
        !std::is_convertible_v<std::decay_t<F>, void(*)()>>, typename ... A>
    once_closure(F&& f, A&& ... a) {
        struct S {
            S(F&& f, A&& ... a)
                : _f(std::forward<F>(f)), _a(std::forward<A>(a)...) {
                static_assert(offsetof(Header, magic) == 0);
                _header.magic = 0xdeadbeef;
                _header.off = offsetof(Header, fp);
                _header.fp = [](void* p) {
                    S* const s = (S*)p;
                    std::apply(s->_f, s->_a);
                    s->~S();
                    co::free(s, sizeof(S));
                };
            }

            ~S() = default;

            struct Header {
                uint32 magic;
                uint32 off;
                void (*fp)(void*);
            } _header;
            std::decay_t<F> _f;
            std::tuple<std::decay_t<A>...> _a;
        };

        _p = co::alloc(sizeof(S), alignof(S));
        runtime_assert(_p);
        new (_p) S(std::forward<F>(f), std::forward<A>(a)...);
    }

    ~once_closure() = default;

    void operator()() {
        typedef void (*_F)();
        typedef void (*_FP)(void*);
        void* const p = (void*)_p;
        if (p) {
            _p = 0;
            if (((size_t)p & 15) != 0 || *(uint32*)p != 0xdeadbeef) {
                ((_F)p)();
            } else {
                (*(_FP*)((char*)p + ((uint32*)p)[1]))(p);
            }
        }
    }

    explicit operator bool() const noexcept {
        return _p != nullptr;
    }

    void* _p;
};

// closure can be called repeatedly
struct closure {
    using _F = void (*)();
    using _FP = void (*)(void*);

    constexpr closure() noexcept : _p(0) {}
    closure(void (*f)()) noexcept : _p((void*)f) {}

    closure(closure&& c) noexcept
        : _p(c._p) {
        c._p = 0;
    }

    closure& operator=(closure&& c) noexcept {
        if (&c != this) {
            closure x(std::move(*this));
            new (this) closure(std::move(c));
        }
        return *this;
    }

    closure(const closure&) = delete;
    void operator=(const closure&) = delete;

    template<typename F, typename = std::enable_if_t<
        !std::is_convertible_v<std::decay_t<F>, void(*)()>>, typename ... A>
    closure(F&& f, A&& ... a) {
        struct S {
            S(F&& f, A&& ... a)
                : _f(std::forward<F>(f)), _a(std::forward<A>(a)...) {
                static_assert(offsetof(Header, magic) == 0);
                _header.magic = 0xdeadbeef;
                _header.off = offsetof(Header, fp);
                _header.fp = [](void* p) {
                    S* const s = (S*) ((size_t)p & ~(size_t)1);
                    if (((size_t)p & 1) == 0) {
                        std::apply(s->_f, s->_a);
                    } else {
                        s->~S();
                        co::free(s, sizeof(S));
                    }
                };
            }

            ~S() = default;

            struct Header {
                uint32 magic;
                uint32 off;
                void (*fp)(void*);
            } _header;
            std::decay_t<F> _f;
            std::tuple<std::decay_t<A>...> _a;
        };

        _p = co::alloc(sizeof(S), alignof(S));
        runtime_assert(_p);
        new (_p) S(std::forward<F>(f), std::forward<A>(a)...);
    }

    ~closure() {
        void* const p = _p;
        if (p) {
            if (((size_t)p & 15) == 0 && *(uint32*)p == 0xdeadbeef) {
                (*(_FP*)((char*)p + ((uint32*)p)[1]))((void*)((size_t)p | 1));
            }
            _p = 0;
        }
    }

    void operator()() {
        void* const p = _p;
        if (p) {
            if (((size_t)p & 15) != 0 || *(uint32*)p != 0xdeadbeef) {
                ((_F)p)();
            } else {
                (*(_FP*)((char*)p + ((uint32*)p)[1]))(p);
            }
        }
    }

    explicit operator bool() const noexcept {
        return _p != nullptr;
    }

    void* _p;
};

} // co
