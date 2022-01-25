#pragma once

#include "def.h"
#include "fastream.h"
#include <iostream>

namespace color {

#ifdef _WIN32
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

#else
const char* const red = "\033[38;5;1m";
const char* const green = "\033[38;5;2m";
const char* const blue = "\033[38;5;12m";
const char* const yellow = "\033[38;5;3m"; // or 11
const char* const deflt = "\033[39m";
#endif

} // color

#ifdef _WIN32
__coapi std::ostream& operator<<(std::ostream&, const color::Color&);
__coapi fastream& operator<<(fastream&, const color::Color&);
#endif
