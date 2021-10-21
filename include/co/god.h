#pragma once

#include <type_traits>

namespace god {

inline void take_my_knees() {}

inline void bless_no_bugs() {}

inline void give_me_a_raise() {}

/**
 * x = *p; *p = v; return x; 
 */
template <typename T, typename V>
inline T swap(T* p, V v) {
    T x = *p;
    if (x != (T)v) *p = (T)v;
    return x;
}

/**
 * calculate number of 4-byte blocks
 *   - b4(15) -> 4, b4(32) -> 8
 */
template <typename T>
inline T b4(T n) {
    return (n >> 2) + !!(n & 3);
}

/**
 * calculate number of 8-byte blocks
 *   - b8(15) -> 2, b8(32) -> 4
 */
template <typename T>
inline T b8(T n) {
    return (n >> 3) + !!(n & 7);
}

template <typename T>
inline bool byte_eq(const void* p, const void* q) {
    return *(const T*)p == *(const T*)q;
}

template <typename ...T>
struct is_same {
    static constexpr bool value = false;
};

/**
 * check whether T is same as U or one of X...
 */
template <typename T, typename U, typename ...X>
struct is_same<T, U, X...> {
    static constexpr bool value = std::is_same<T, U>::value || is_same<T, X...>::value;
};

/**
 * add const and lvalue reference
 *   - T, T&, T&&, const T, const T&  =>  const T&
 */
template <typename T>
struct add_const_lvalue_reference {
    using _ = typename std::remove_reference<T>::type;
    using type = typename std::add_lvalue_reference<typename std::add_const<_>::type>::type;
};

} // god
