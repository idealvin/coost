#pragma once

#include <stddef.h>
#include <utility>
#include <type_traits>

namespace god {

inline void bless_no_bugs() {}

// universal type conversion
//   - int x = god::cast<int>(1.23);
//   - X x; Y& y = god::cast<Y&>(x);
template<typename To, typename From>
constexpr To cast(From&& f) {
    return (To) std::forward<From>(f);
}

// align integer up to @A
//   - god::align_up<64>(123); -> 128
template<size_t A, typename X>
constexpr X align_up(X x) {
    static_assert((A & (A - 1)) == 0, "A must be power of 2");
    return (x + static_cast<X>(A - 1)) & ~static_cast<X>(A - 1);
}

// align pointer up to @A
template<size_t A, typename X>
constexpr X* align_up(X* x) {
    return (X*) align_up<A>((size_t)x);
}

// align integer up to @A
//   - @a  align, must be power of 2
//   - god::align_up(123, 64); -> 128
template<typename X, typename A>
constexpr X align_up(X x, A a) {
    return (x + static_cast<X>(a - 1)) & ~static_cast<X>(a - 1);
}

// align pointer up to @A
//   - @a  align, must be power of 2
template<typename X, typename A>
constexpr X* align_up(X* x, A a) {
    return (X*) align_up((size_t)x, a);
}

// align integer down to @A
//   - god::align_down<64>(123); -> 64
template<size_t A, typename X>
constexpr X align_down(X x) {
    static_assert((A & (A - 1)) == 0, "");
    return x & ~static_cast<X>(A - 1);
}

// align pointer down to @A
template<size_t A, typename X>
constexpr X* align_down(X* x) {
    return (X*) align_down<A>((size_t)x);
}

// align integer down to @A
//   - @a  align, must be power of 2
//   - god::align_down<64>(123); -> 64
template<typename X, typename A>
constexpr X align_down(X x, A a) {
    return x & ~static_cast<X>(a - 1);
}

// align pointer down to @A
//   - @a  align, must be power of 2
template<typename X, typename A>
constexpr X* align_down(X* x, A a) {
    return (X*) align_down((size_t)x, a);
}

template<typename T>
constexpr T log2(T x) {
    return x <= 1 ? 0 : 1 + god::log2(x >> 1);
}

// n blocks
//   - @N  block size, must be power of 2
//   - god::nb<16>(32); -> 2
//   - god::nb<32>(33); -> 2
template<size_t N, typename X>
constexpr X nb(X x) {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");
    return (x >> god::log2(static_cast<X>(N))) + !!(x & static_cast<X>(N - 1));
}

// if the values of type T stored in @p and @q are equal
//   - god::eq<uint32>("nicecode", "nicework"); -> true
//   - god::eq<uint64>("nicecode", "nicework"); -> false
template<typename T>
inline bool eq(const void* p, const void* q) {
    return *(const T*)p == *(const T*)q;
}

// copy N bytes from @src to @dst
template<size_t N>
inline void copy(void* dst, const void* src) {
    copy<N - 1>(dst, src);
    *((char*)dst + N - 1) = *((const char*)src + N - 1);
}

template<>
inline void copy<0>(void*, const void*) {}

// x = *p; *p = v; return x; 
template<typename T, typename V>
inline T swap(T* p, V v) {
    const T x = *p;
    *p = (T)v;
    return x;
}

template<typename T, typename V>
inline T fetch_add(T* p, V v) {
    const T x = *p;
    *p += v;
    return x;
}

template<typename T, typename V>
inline T fetch_sub(T* p, V v) {
    const T x = *p;
    *p -= v;
    return x;
}

template<typename T, typename V>
inline T fetch_and(T* p, V v) {
    const T x = *p;
    *p &= (T)v;
    return x;
}

template<typename T, typename V>
inline T fetch_or(T* p, V v) {
    const T x = *p;
    *p |= (T)v;
    return x;
}

template<typename T, typename V>
inline T fetch_xor(T* p, V v) {
    const T x = *p;
    *p ^= (T)v;
    return x;
}

template<bool C, typename T=void>
using if_t = typename std::enable_if<C, T>::type;

// remove lvalue and rvalue reference
template<typename T>
using rm_ref_t = typename std::remove_reference<T>::type;

// remove const and volatile
template<typename T>
using rm_cv_t = typename std::remove_cv<T>::type;

// remove reference, const and volatile
template<typename T>
using rm_cvref_t = rm_cv_t<rm_ref_t<T>>;

// remove the first array dimension
//   - god::rm_arr_t<int[6][8]>  ->  int[8]
//   - god::rm_arr_t<int[8]>     ->  int
template<typename T>
using rm_arr_t = typename std::remove_extent<T>::type;

template<typename T>
using _const_t = typename std::add_const<T>::type;

// const lvalue reference
//   - T, T&, T&&, const T, const T&  =>  const T&
template<typename T>
using const_ref_t = typename std::add_lvalue_reference<_const_t<rm_ref_t<T>>>::type;

namespace xx {
template<typename ...T>
struct is_same {
    static constexpr bool value = false;
};

template<typename T, typename U, typename ...X>
struct is_same<T, U, X...> {
    static constexpr bool value = std::is_same<T, U>::value || is_same<T, X...>::value;
};
} // xx

// check if T is same as U or one of X...
template<typename T, typename U, typename ...X>
constexpr bool is_same() {
    return xx::is_same<T, U, X...>::value;
};

template<typename T>
constexpr bool is_ref() {
    return std::is_reference<T>::value;
}

template<typename T>
constexpr bool is_array() {
    return std::is_array<T>::value;
}

template<typename T>
constexpr bool is_class() {
    return std::is_class<T>::value;
}

template<typename T>
constexpr bool is_scalar() {
    return std::is_scalar<T>::value;
}

#if defined(__GNUC__) && __GNUC__ < 5
template<typename T>
constexpr bool is_trivially_copyable() {
    return __has_trivial_copy(T);
}
#else
template<typename T>
constexpr bool is_trivially_copyable() {
    return std::is_trivially_copyable<T>::value;
}
#endif

template<typename T>
constexpr bool is_trivially_destructible() {
    return std::is_trivially_destructible<T>::value;
}

// if B is base class of D
template<typename B, typename D>
constexpr bool is_base_of() {
    return std::is_base_of<B, D>::value;
}

template<typename T>
constexpr bool has_virtual_destructor() {
    return std::has_virtual_destructor<T>::value;
}

} // god

// detect whether a class has a specified method
//   - https://stackoverflow.com/a/257382/4984605
//   - e.g. 
//     DEF_has_method(c_str);
//     god::has_method_c_str<fastring>(); // -> true
#define DEF_has_method(f) \
namespace god { \
template<typename _T_> \
struct _has_method_##f { \
    struct _R_ { int _[2]; }; \
    template<typename _X_> static int test(decltype(&_X_::f)); \
    template<typename _X_> static _R_ test(...); \
    enum { value = sizeof(test<god::rm_cvref_t<_T_>>(0)) == sizeof(int) }; \
}; \
\
template<typename _T_> \
constexpr bool has_method_##f() noexcept { return _has_method_##f<_T_>::value; } \
}
