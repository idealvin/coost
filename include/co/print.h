#pragma once

#include "string.h"
#include <utility>

namespace co {
namespace xx {

struct PrintInit {
    PrintInit();
    ~PrintInit() = default;
};

static PrintInit g_print_init;

struct color {
    struct _text {
        constexpr _text(const char* s, int c) noexcept
            : s(s), c(c) {
        }
        const char* s;
        int c;
    };

    static _text deflt         (const char* s) { return _text(s, 0); }
    static _text red           (const char* s) { return _text(s, 1); }
    static _text green         (const char* s) { return _text(s, 2); }
    static _text yellow        (const char* s) { return _text(s, 3); }
    static _text blue          (const char* s) { return _text(s, 4); }
    static _text magenta       (const char* s) { return _text(s, 5); }
    static _text cyan          (const char* s) { return _text(s, 6); }
    static _text bold          (const char* s) { return _text(s, 8); }
    static _text bright_red    (const char* s) { return _text(s, 9); }
    static _text bright_green  (const char* s) { return _text(s, 10); }
    static _text bright_yellow (const char* s) { return _text(s, 11); }
    static _text bright_blue   (const char* s) { return _text(s, 12); }
    static _text bright_magenta(const char* s) { return _text(s, 13); }
    static _text bright_cyan   (const char* s) { return _text(s, 14); }
};

struct stream {
    stream() = default;
    ~stream() { this->flush(); }

    void flush();

private:
    co::string _s;

    stream& operator<<(color::_text&& t);
    stream& operator<<(color::_text (*f)(const char*)) {
        return this->operator<<(f(nullptr));
    }

    template<typename T>
    stream& operator<<(T&& t) {
        _s << std::forward<T>(t);
        return *this;
    }

    friend struct Print;
    friend struct PrintLn;
};

struct Print {
    Print();
    ~Print();

    stream& print() { return _s; }

    template<typename X, typename ...V>
    stream& print(X&& x, V&& ... v) {
        _s << std::forward<X>(x);
        return this->print(std::forward<V>(v)...);
    }

    stream& _s;
};

struct PrintLn {
    PrintLn();
    ~PrintLn();

    stream& print() { return _s; }

    template<typename X, typename ...V>
    stream& print(X&& x, V&& ... v) {
        _s << std::forward<X>(x);
        return this->print(std::forward<V>(v)...);
    }

    stream& _s;
    size_t _n;
};

} // xx

using xx::color;

// print to console, thread-safe
template<typename ...X>
inline xx::stream& print(X&& ...x) {
    return xx::Print().print(std::forward<X>(x)...);
}

// print to console with newline, thread-safe
template<typename ...X>
inline void println(X&& ...x) {
    xx::PrintLn().print(std::forward<X>(x)...);
}

} // co
