#pragma once

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

} // god
