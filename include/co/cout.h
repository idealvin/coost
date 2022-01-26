#pragma once

#include "fastream.h"
#include <iostream>

using std::cout;
using std::endl;

#ifdef _WIN32
namespace color {

struct __coapi Color {
    Color(const char* s, int i);

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

__coapi std::ostream& operator<<(std::ostream&, const color::Color&);

#else /* unix */
namespace color {

const char* const red = "\033[38;5;1m";
const char* const green = "\033[38;5;2m";
const char* const blue = "\033[38;5;12m";
const char* const yellow = "\033[38;5;3m"; // or 11
const char* const deflt = "\033[39m";

} // color
#endif

namespace co {
namespace xx {

struct __coapi Cout {
    Cout();
    Cout(const char* file, unsigned int line);
    ~Cout();

    fastream& stream() {
        static fastream kStream(128);
        return kStream;
    }
};

} // xx
} // co

// thread-safe, but may not support color output on Windows
#define COUT   co::xx::Cout().stream()
#define CLOG   co::xx::Cout(__FILE__, __LINE__).stream()
