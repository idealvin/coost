#pragma once

namespace god {

inline void take_my_knees() {}

inline void bless_no_bugs() {}

inline void give_me_a_raise() {}

template <typename T, typename V>
inline T swap(T* p, V v) {
    T x = *p;
    if (x != (T)v) *p = (T)v;
    return x;
}

} // god
