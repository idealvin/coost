#pragma once

#include <utility>
#include <type_traits>

namespace co {

template <typename F>
struct Defer {
    Defer(F&& f) : _f(std::forward<F>(f)) {}
    ~Defer() { _f(); }
    typename std::remove_reference<F>::type _f;
};

template <typename F>
inline Defer<F> create_defer(F&& f) {
    return Defer<F>(std::forward<F>(f));
}

#define _co_defer_name_cat(x, n) x##n
#define _co_defer_make_name(x, n) _co_defer_name_cat(x, n)
#define _co_defer_name _co_defer_make_name(_co_defer_, __LINE__)

} // co

#define defer(e) auto _co_defer_name = co::create_defer([&](){ e; })
