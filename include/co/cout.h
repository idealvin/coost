#pragma once

#include "fastream.h"
#include <iostream>

using std::cout;
using std::endl;

#ifdef _WIN32
namespace color {

struct __coapi Color {
    Color(const char* s, int i);
    Color(const Color& c) noexcept : s(c.s) {}

    union {
        int i;
        const char* s;
    };
};

__coapi extern const Color red;
__coapi extern const Color green;
__coapi extern const Color blue;
__coapi extern const Color yellow;
__coapi extern const Color deflt; // default color

} // color

namespace text {

struct Text {
    Text(const char* s, size_t n, const color::Color& c) noexcept
        : s(s), n(n), c(c) {
    }
    const char* s;
    size_t n;
    color::Color c;
};

} // text


__coapi std::ostream& operator<<(std::ostream&, const color::Color&);

#else /* unix */
namespace color {

const char* const red = "\033[38;5;1m";
const char* const green = "\033[38;5;2m";
const char* const blue = "\033[38;5;12m";
const char* const yellow = "\033[38;5;3m"; // or 11
const char* const deflt = "\033[39m";

} // color

namespace text {

struct Text {
    constexpr Text(const char* s, size_t n, const char* c) noexcept
        : s(s), n(n), c(c) {
    }
    const char* s;
    size_t n;
    const char* c;
};

} // text

#endif

namespace text {

inline Text red(const char* s, size_t n) {
    return Text(s, n, color::red);
}

inline Text red(const char* s) {
    return red(s, strlen(s));
}

inline Text green(const char* s, size_t n) {
    return Text(s, n, color::green);
}

inline Text green(const char* s) {
    return green(s, strlen(s));
}

inline Text blue(const char* s, size_t n) {
    return Text(s, n, color::blue);
}

inline Text blue(const char* s) {
    return blue(s, strlen(s));
}

inline Text yellow(const char* s, size_t n) {
    return Text(s, n, color::yellow);
}

inline Text yellow(const char* s) {
    return yellow(s, strlen(s));
}

} // text

inline std::ostream& operator<<(std::ostream& os, const text::Text& t) {
    return (os << t.c).write(t.s, t.n) << color::deflt;
}

namespace co {
namespace xx {

struct __coapi Cout {
    Cout();
    Cout(const char* file, unsigned int line);
    ~Cout();

    fastream& stream();
    size_t _n;
};

} // xx
} // co

// thread-safe, but may not support color output on Windows
#define COUT   co::xx::Cout().stream()
#define CLOG   co::xx::Cout(__FILE__, __LINE__).stream()
