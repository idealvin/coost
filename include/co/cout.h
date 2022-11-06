#pragma once

#include "fastream.h"
#include "stref.h"
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
__coapi extern const Color magenta;
__coapi extern const Color cyan;
__coapi extern const Color none;
__coapi extern const Color bold;
__coapi extern const Color deflt; // default color

} // color

#else /* unix */
namespace color {

const char red[] = "\033[31m";     // "\033[38;5;1m";
const char green[] = "\033[32m";   // "\033[38;5;2m";
const char blue[] = "\033[34m";    // "\033[38;5;4m";
const char yellow[] = "\033[33m";  // "\033[38;5;3m"; // or 11
const char magenta[] = "\033[95m"; // bright magenta
const char cyan[] = "\033[96m";    // "\033[38;5;3m"; // or 11
const char none[] = "";
const char bold[] = "\033[1m";
const char deflt[] = "\033[0m";    //"\033[39m";

typedef const char* Color;

} // color

#endif

namespace text {

struct Text {
    Text(const char* s, size_t n, const color::Color& c) noexcept
        : s(s), n(n), c(c) {
    }
    const char* s;
    size_t n;
    color::Color c;
};

inline Text red(const co::stref& s) {
    return Text(s.data(), s.size(), color::red);
}

inline Text green(const co::stref& s) {
    return Text(s.data(), s.size(), color::green);
}

inline Text blue(const co::stref& s) {
    return Text(s.data(), s.size(), color::blue);
}

inline Text yellow(const co::stref& s) {
    return Text(s.data(), s.size(), color::yellow);
}

inline Text magenta(const co::stref& s) {
    return Text(s.data(), s.size(), color::magenta);
}

inline Text cyan(const co::stref& s) {
    return Text(s.data(), s.size(), color::cyan);
}

struct Bold {
    Bold(const char* s, size_t n) noexcept : _s(s), _n(n), _c(color::none) {}
    Bold& red() noexcept { _c = color::red; return *this; }
    Bold& green() noexcept { _c = color::green; return *this; }
    Bold& blue() noexcept { _c = color::blue; return *this; }
    Bold& yellow() noexcept { _c = color::yellow; return *this; }
    Bold& magenta() noexcept { _c = color::magenta; return *this; }
    Bold& cyan() noexcept { _c = color::cyan; return *this; }

    const char* _s;
    size_t _n;
    color::Color _c;
};

inline Bold bold(const co::stref& s) noexcept {
    return Bold(s.data(), s.size());
}

} // text

#ifdef _WIN32
__coapi std::ostream& operator<<(std::ostream&, const color::Color&);
__coapi std::ostream& operator<<(std::ostream&, const text::Text&);
__coapi std::ostream& operator<<(std::ostream&, const text::Bold&);

#else
inline std::ostream& operator<<(std::ostream& os, const text::Text& t) {
    return (os << t.c).write(t.s, t.n) << color::deflt;
}

inline std::ostream& operator<<(std::ostream& os, const text::Bold& b) {
    return (os << color::bold << b._c).write(b._s, b._n) << color::deflt;
}
#endif


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
