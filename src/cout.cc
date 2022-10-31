#include "co/cout.h"
#include <mutex>

#ifdef _WIN32
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

inline HANDLE std_handle() {
    static HANDLE h = []() {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        return (h != INVALID_HANDLE_VALUE && h != NULL) ? h : (HANDLE)NULL;
    }();
    return h;
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

std::ostream& operator<<(std::ostream& os, const color::Color& c) {
    if (color::ansi_color_seq_enabled()) {
        return os << c.s;
    } else {
        auto h = color::std_handle();
        if (h) SetConsoleTextAttribute(h, (WORD)c.i);
        return os;
    }
}
#endif

namespace co {
namespace xx {

inline std::recursive_mutex& mutex() {
    static auto kmtx = []() {
        static char kbuf[sizeof(std::recursive_mutex)];
        return new (kbuf) std::recursive_mutex();
    }();
    return *kmtx;
}

Cout::Cout() {
    mutex().lock();
    _n = this->stream().size();
}

Cout::Cout(const char* file, unsigned int line) {
    mutex().lock();
    _n = this->stream().size();
    this->stream() << file << ':' << line << ']' << ' ';
}

Cout::~Cout() {
    auto& s = this->stream().append('\n');
    ::fwrite(s.data() + _n, 1, s.size() - _n, stderr);
    s.resize(_n);
    mutex().unlock();
}

fastream& Cout::stream() {
    static auto ks = []() {
        static char kbuf[sizeof(fastream)];
        return new (kbuf) fastream(256);
    }();
    return *ks;
}

} // xx
} // co
