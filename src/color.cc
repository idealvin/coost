#ifdef _WIN32
#include "co/color.h"
#include "co/os.h"

#ifdef _MSC_VER
#pragma warning(disable:4503)
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace color {

inline bool ansi_color_seq_enabled() {
    static const bool x = !os::env("TERM").empty();
    return x;
}

inline HANDLE& std_handle() {
    static HANDLE handle = []() {
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (handle != INVALID_HANDLE_VALUE && handle != NULL) return handle;
        return (HANDLE)NULL;
    }();
    return handle;
}

inline int get_default_color() {
    auto h = std_handle();
    if (!h) return 15;

    CONSOLE_SCREEN_BUFFER_INFO buf;
    if (!GetConsoleScreenBufferInfo(h, &buf)) return 15;
    return buf.wAttributes & 0x0f;
}

Color::Color(const char* ansi_seq, int win_color) {
    ansi_color_seq_enabled() ? (void)(s = ansi_seq) : (void)(i = win_color);
}

const Color red("\033[38;5;1m", FOREGROUND_RED);     // 12
const Color green("\033[38;5;2m", FOREGROUND_GREEN); // 10
const Color blue("\033[38;5;12m", FOREGROUND_BLUE);  // 9
const Color yellow("\033[38;5;11m", 14);
const Color deflt("\033[39m", get_default_color());

} // color

std::ostream& operator<<(std::ostream& os, const color::Color& color) {
    if (color::ansi_color_seq_enabled()) {
        os << color.s;
        return os;
    } else {
        auto h = color::std_handle();
        if (h) SetConsoleTextAttribute(h, (WORD)color.i);
        return os;
    }
}

fastream& operator<<(fastream& os, const color::Color& color) {
    if (color::ansi_color_seq_enabled()) {
        os << color.s;
        return os;
    } else {
        auto h = color::std_handle();
        if (h) SetConsoleTextAttribute(h, (WORD)color.i);
        return os;
    }
}

#endif
