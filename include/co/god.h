#pragma once

#include <type_traits>

namespace god {

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

/**
 * whether the first sizeof(T) bytes are equal
 */
template <typename T>
inline bool bytes_eq(const void* p, const void* q) {
    return *(const T*)p == *(const T*)q;
}

// remove lvalue or rvalue reference
template <typename T>
using remove_ref_t = typename std::remove_reference<T>::type;

// remove const or volatile
template <typename T>
using remove_cv_t = typename std::remove_cv<T>::type;

// remove reference and const/volatile
template <typename T>
using remove_cvref_t = remove_cv_t<remove_ref_t<T>>;

// remove the first array dimension
template <typename T>
using remove_arr_t = typename std::remove_extent<T>::type;

template <typename T>
using add_const_t = typename std::add_const<T>::type;

/**
 * const lvalue reference
 *   - T, T&, T&&, const T, const T&  =>  const T&
 */
template <typename T>
using const_ref_t = typename std::add_lvalue_reference<add_const_t<remove_ref_t<T>>>::type;

namespace xx {
template <typename ...T>
struct is_same {
    static constexpr bool value = false;
};

template <typename T, typename U, typename ...X>
struct is_same<T, U, X...> {
    static constexpr bool value = std::is_same<T, U>::value || is_same<T, X...>::value;
};
} // xx

// check whether T is same as U or one of X...
template <typename T, typename U, typename ...X>
inline constexpr bool is_same() {
    return xx::is_same<T, U, X...>::value;
};

// check whether T is pointer, member pointer or std::nullptr_t
template <typename T>
inline constexpr bool is_pointer() {
    return std::is_pointer<T>::value || std::is_member_pointer<T>::value || 
           std::is_same<typename std::remove_cv<T>::type, decltype(nullptr)>::value;
}

template <typename T>
inline constexpr bool is_ref() {
    return std::is_reference<T>::value;
}

template <typename T>
inline constexpr bool is_array() {
    return std::is_array<T>::value;
}

template <typename T>
inline constexpr bool is_class() {
    return std::is_class<T>::value;
}

template <typename T>
inline constexpr bool is_scalar() {
    return std::is_scalar<T>::value;
}

template <typename T>
inline constexpr bool is_trivially_copyable() {
    return std::is_trivially_copyable<T>::value;
}

template <typename T>
inline constexpr bool is_trivially_destructible() {
    return std::is_trivially_destructible<T>::value;
}

template <bool C, typename T=void>
using enable_if_t = typename std::enable_if<C, T>::type;

} // god
