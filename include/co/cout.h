#pragma once

#include "fastream.h"
#include <iostream>

using std::cout;
using std::endl;

namespace co {
namespace color {

enum Color {
    deflt = 0,
    red = 1,
    green = 2,
    yellow = 3,  // red | green
    blue = 4,
    magenta = 5, // blue | red
    cyan = 6,    // blue | green
    bold = 8,
};

} // color

namespace text {

struct Text {
    constexpr Text(const char* s, size_t n, color::Color c) noexcept
        : s(s), n(n), c(c) {
    }
    const char* s;
    size_t n;
    color::Color c;
};

inline Text red(const anystr& s) noexcept {
    return Text(s.data(), s.size(), color::red);
}

inline Text green(const anystr& s) noexcept {
    return Text(s.data(), s.size(), color::green);
}

inline Text blue(const anystr& s) noexcept {
    return Text(s.data(), s.size(), color::blue);
}

inline Text yellow(const anystr& s) noexcept {
    return Text(s.data(), s.size(), color::yellow);
}

inline Text magenta(const anystr& s) noexcept {
    return Text(s.data(), s.size(), color::magenta);
}

inline Text cyan(const anystr& s) noexcept {
    return Text(s.data(), s.size(), color::cyan);
}

struct Bold {
    constexpr Bold(const char* s, size_t n) noexcept
        : s(s), n(n), c(color::bold) {
    }
    Bold& red() noexcept { i |= color::red; return *this; }
    Bold& green() noexcept { i |= color::green; return *this; }
    Bold& blue() noexcept { i |= color::blue; return *this; }
    Bold& yellow() noexcept { i |= color::yellow; return *this; }
    Bold& magenta() noexcept { i |= color::magenta; return *this; }
    Bold& cyan() noexcept { i |= color::cyan; return *this; }
    const char* s;
    size_t n;
    union {
        int i;
        color::Color c;
    };
};

inline Bold bold(const anystr& s) noexcept {
    return Bold(s.data(), s.size());
}

} // text
} // co

namespace color = co::color;
namespace text = co::text;

__coapi std::ostream& operator<<(std::ostream&, color::Color);
__coapi fastream& operator<<(fastream&, color::Color);

inline std::ostream& operator<<(std::ostream& os, const text::Text& x) {
    return (os << x.c).write(x.s, x.n) << color::deflt;
}

inline std::ostream& operator<<(std::ostream& os, const text::Bold& x) {
    return (os << x.c).write(x.s, x.n) << color::deflt;
}

inline fastream& operator<<(fastream& os, const text::Text& x) {
    return (os << x.c).append(x.s, x.n) << color::deflt;
}

inline fastream& operator<<(fastream& os, const text::Bold& x) {
    return (os << x.c).append(x.s, x.n) << color::deflt;
}


namespace co {
namespace xx {

struct __coapi Cout {
    Cout();
    ~Cout();
    fastream& s;
    size_t n;
};

} // xx

// print to stdout with newline (thread-safe)
//   - co::print("hello", text::green(" xxx "), 23);
template<typename ...X>
inline void print(X&& ... x) {
    xx::Cout().s.cat(std::forward<X>(x)...);
}

} // co
