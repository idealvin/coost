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

inline bool ansi_esc_seq_enabled() {
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

inline int get_current_color() {
    CONSOLE_SCREEN_BUFFER_INFO buf;
    auto h = std_handle();
    if (h && GetConsoleScreenBufferInfo(h, &buf)) {
        return buf.wAttributes & 0x0f;
    } 
    return 15;
}

Color::Color(const char* ansi_seq, int win_color) {
    ansi_esc_seq_enabled() ? (void)(s = ansi_seq) : (void)(i = win_color);
}

const Color red("\033[31m", FOREGROUND_RED);     // 4
const Color green("\033[32m", FOREGROUND_GREEN); // 2
const Color blue("\033[34m", FOREGROUND_BLUE);   // 1
const Color yellow("\033[33m", FOREGROUND_RED | FOREGROUND_GREEN);
const Color magenta("\033[95m", FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
const Color cyan("\033[96m", FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
const Color none("", 0);
const Color bold("\033[1m", FOREGROUND_INTENSITY);
const Color deflt("\033[0m", get_current_color());

} // color

std::ostream& operator<<(std::ostream& os, const color::Color& c) {
    if (color::ansi_esc_seq_enabled()) {
        return os << c.s;
    } else {
        auto h = color::std_handle();
        if (h && c.i) SetConsoleTextAttribute(h, (WORD)c.i);
        return os;
    }
}

std::ostream& operator<<(std::ostream& os, const text::Text& t) {
    if (color::ansi_esc_seq_enabled()) {
        return (os << t.c).write(t.s, t.n) << color::deflt;
    } else {
        return (os.flush() << t.c).write(t.s, t.n).flush() << color::deflt; 
    }
}

std::ostream& operator<<(std::ostream& os, const text::Bold& b) {
    if (color::ansi_esc_seq_enabled()) {
        return (os << "\033[1m" << b._c).write(b._s, b._n) << color::deflt;
    } else {
        ((text::Bold&)b)._c.i |= FOREGROUND_INTENSITY;
        return (os.flush() << b._c).write(b._s, b._n).flush() << color::deflt; 
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
