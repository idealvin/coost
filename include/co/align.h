#pragma once

#include <stddef.h>
#include <type_traits>

namespace co {

// align_up<64>(123); -> 128
template<size_t A, typename X, typename=std::enable_if_t<std::is_integral_v<X>>>
constexpr X align_up(X x) noexcept {
    static_assert(A > 0 && (A & (A - 1)) == 0, "A must be power of 2");
    using U = std::make_unsigned_t<X>;
    constexpr U mask = static_cast<U>(A - 1);
    return static_cast<X>((static_cast<U>(x) + mask) & ~mask);
}

template<size_t A, typename X>
constexpr X* align_up(X* x) noexcept {
    return (X*) align_up<A>((size_t)x);
}

// @a must be power of 2
//   - align_up(123, 64); -> 128
template<typename X, typename A,
    typename=std::enable_if_t<std::is_integral_v<X> && std::is_integral_v<A>>>
constexpr X align_up(X x, A a) noexcept {
    using U = std::make_unsigned_t<X>;
    return static_cast<X>(((U)x + (U)(a - 1)) & ~(U)(a - 1));
}

// @a must be power of 2
template<typename X, typename A, typename=std::enable_if_t<std::is_integral_v<A>>>
constexpr X* align_up(X* x, A a) noexcept {
    return (X*) align_up((size_t)x, a);
}

// align_down<64>(123); -> 64
template<size_t A, typename X, typename=std::enable_if_t<std::is_integral_v<X>>>
constexpr X align_down(X x) noexcept {
    static_assert(A > 0 && (A & (A - 1)) == 0, "A must be power of 2");
    using U = std::make_unsigned_t<X>;
    return static_cast<X>(static_cast<U>(x) & ~static_cast<U>(A - 1));
}

template<size_t A, typename X>
constexpr X* align_down(X* x) noexcept {
    return (X*) align_down<A>((size_t)x);
}

// @a must be power of 2
//   - align_down(123, 64); -> 64
template<typename X, typename A,
    typename=std::enable_if_t<std::is_integral_v<X> && std::is_integral_v<A>>>
constexpr X align_down(X x, A a) noexcept {
    using U = std::make_unsigned_t<X>;
    return static_cast<X>(static_cast<U>(x) & ~static_cast<U>(a - 1));
}

template<typename X, typename A, typename=std::enable_if_t<std::is_integral_v<A>>>
constexpr X* align_down(X* x, A a) noexcept {
    return (X*) align_down((size_t)x, a);
}

} // co
