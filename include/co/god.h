#pragma once

#include "def.h"
#include <type_traits>

namespace god {

inline void bless_no_bugs() {}

inline void give_me_a_raise() {}

// x = *p; *p = v; return x; 
template <typename T, typename V>
inline T swap(T* p, V v) {
    const T x = *p;
    *p = (T)v;
    return x;
}

template <typename T, typename V>
inline T fetch_add(T* p, V v) {
    const T x = *p;
    *p += v;
    return x;
}

template <typename T, typename V>
inline T fetch_sub(T* p, V v) {
    const T x = *p;
    *p -= v;
    return x;
}

template <typename T, typename V>
inline T fetch_and(T* p, V v) {
    const T x = *p;
    *p &= (T)v;
    return x;
}

template <typename T, typename V>
inline T fetch_or(T* p, V v) {
    const T x = *p;
    *p |= (T)v;
    return x;
}

template <typename T, typename V>
inline T fetch_xor(T* p, V v) {
    const T x = *p;
    *p ^= (T)v;
    return x;
}

// @align must be power of 2
template <typename X, typename A>
inline X align_up(X x, A align) {
    const X o = static_cast<X>(align) - 1;
    return (x + o) & ~o;
}

template <size_t A, typename X>
inline X align_up(X x) {
    const X o = static_cast<X>(A) - 1;
    return (x + o) & ~o;
}

template <typename X, typename A>
inline X* align_up(X* x, A align) {
    return (X*) align_up((size_t)x, align);
}

template <size_t A, typename X>
inline X* align_up(X* x) {
    return (X*) align_up<A>((size_t)x);
}

// @align must be power of 2
template <typename X, typename A>
inline X align_down(X x, A align) {
    const X o = static_cast<X>(align) - 1;
    return x & ~o;
}

template <size_t A, typename X>
inline X align_down(X x) {
    const X o = static_cast<X>(A) - 1;
    return x & ~o;
}

template <typename X, typename A>
inline X* align_down(X* x, A align) {
    return (X*) align_down((size_t)x, align);
}

template <size_t A, typename X>
inline X* align_down(X* x) {
    return (X*) align_down<A>((size_t)x);
}

// b4(15) -> 4, b4(32) -> 8
template <typename T>
inline T b4(T n) {
    return (n >> 2) + !!(n & 3);
}

// b8(15) -> 2, b8(32) -> 4
template <typename T>
inline T b8(T n) {
    return (n >> 3) + !!(n & 7);
}

// b16(15) -> 1, b16(32) -> 2
template <typename T>
inline T b16(T n) {
    return (n >> 4) + !!(n & 15);
}

// b4k(4095) -> 1, b4k(8192) -> 2
template <typename T>
inline T b4k(T n) {
    return (n >> 12) + !!(n & 4095);
}

// whether the first sizeof(T) bytes are equal
template <typename T>
inline bool byte_eq(const void* p, const void* q) {
    return *(const T*)p == *(const T*)q;
}

template <int N>
inline void byte_cp(void* dst, const void* src) {
    byte_cp<N - 1>(dst, src);
    *((char*)dst + N - 1) = *((const char*)src + N - 1);
}

template <>
inline void byte_cp<0>(void* dst, const void* src) {}

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
inline constexpr bool is_rvalue_ref() {
    return std::is_rvalue_reference<T>::value;
}

template <typename T>
inline constexpr bool is_lvalue_ref() {
    return std::is_lvalue_reference<T>::value;
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

// check whether T is one of the basic built-in types like bool, int, double...
template<typename T>
inline constexpr bool is_basic() {
    return god::is_same<god::remove_cv_t<T>,
        bool, char, signed char, unsigned char,
        short, unsigned short, int, unsigned int,
        long, unsigned long, long long, unsigned long long,
        float, double
    >();
}

template<typename T>
inline constexpr bool is_integer() {
    return god::is_same<god::remove_cv_t<T>,
        short, unsigned short, int, unsigned int,
        long, unsigned long, long long, unsigned long long
    >();
}

// check whether T is a literal string like "hello"
template<typename T>
inline constexpr bool is_literal_string() {
    return god::is_array<T>() && god::is_same<god::remove_arr_t<T>, const char>();
}

// check whether T is a c-style string: const char*, char*, char[]
template<typename T>
inline constexpr bool is_c_str() {
    return god::is_same<god::remove_cv_t<T>, const char*, char*>() ||
        (god::is_array<T>() && god::is_same<god::remove_arr_t<T>, char>());
}

#if defined(__GNUC__) && __GNUC__ < 5
template <typename T>
inline constexpr bool is_trivially_copyable() {
    return __has_trivial_copy(T);
}
#else
template <typename T>
inline constexpr bool is_trivially_copyable() {
    return std::is_trivially_copyable<T>::value;
}
#endif

template <typename T>
inline constexpr bool is_trivially_destructible() {
    return std::is_trivially_destructible<T>::value;
}

template <bool C, typename T=void>
using enable_if_t = typename std::enable_if<C, T>::type;

} // god

// detect whether a class has a specified method
// https://stackoverflow.com/a/257382/4984605
#define DEF_has_method(f) \
namespace god { \
template <typename _T_> \
struct _has_method_##f { \
    struct _R_ { int _[2]; }; \
    template <typename _X_> static int test(decltype(&_X_::f)); \
    template <typename _X_> static _R_ test(...); \
    enum { value = sizeof(test<god::remove_cvref_t<_T_>>(0)) == sizeof(int) }; \
}; \
\
template <typename _T_> \
constexpr bool has_method_##f() noexcept { return _has_method_##f<_T_>::value; } \
}
