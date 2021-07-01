#pragma once

#include <utility>

template <typename F>
struct _defer_class {
    _defer_class(F&& f) : _f(std::move(f)) {}
    ~_defer_class() { _f(); }
    F _f;
};

template <typename F>
inline _defer_class<F> _create_defer_class(F&& f) {
    return _defer_class<F>(std::move(f));
}

#define _defer_name_cat(x, n) x##n
#define _defer_name(x, n) _defer_name_cat(x, n)
#define _defer_var_name _defer_name(_defer_var_, __LINE__)

#define defer(e) \
    auto _defer_var_name = _create_defer_class([&](){ e; })
