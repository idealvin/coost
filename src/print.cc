#include "co/print.h"
#include <stdio.h>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace co {
namespace xx {

static const char* fg[16] = {
    "\033[0m",   // default
    "\033[31m",  // red
    "\033[32m",  // green
    "\033[33m",  // yellow
    "\033[34m",  // blue
    "\033[35m",  // magenta
    "\033[36m",  // cyan
    "\033[37m",  // white
    "\033[1m",   // bold
    "\033[1m\033[91m",
    "\033[1m\033[92m", // 32
    "\033[1m\033[93m", // 33
    "\033[1m\033[94m",
    "\033[1m\033[95m",
    "\033[1m\033[96m",
    "\033[1m\033[97m",
};

static __thread stream* g_s;
static __thread stream* g_sln;
static bool g_color_enabled;
static std::mutex* g_mtx;
static int g_nifty_counter;

inline void write_to_console(const char* s, size_t n) {
    std::lock_guard<std::mutex> g(*g_mtx);
    ::fwrite(s, 1, n, stderr);
}

stream& stream::operator<<(color::_text&& t) {
    if (g_color_enabled) _s << fg[t.c];
    if (t.s) {
        _s << t.s;
        if (g_color_enabled) _s << fg[0];
    }
    return *this;
}

void stream::flush() {
    if (!_s.empty()) {
        write_to_console(_s.data(), _s.size());
        _s.clear();
    }
}

PrintInit::PrintInit() {
    if (g_nifty_counter++ == 0) {
    #ifdef _WIN32
        g_color_enabled = []() {
            auto h = GetStdHandle(STD_ERROR_HANDLE);
            if (!h || h == INVALID_HANDLE_VALUE) return false;

            // windows cmd, terminal...
            DWORD mode = 0;
            if (GetConsoleMode(h, &mode)) {
            #ifdef ENABLE_VIRTUAL_TERMINAL_PROCESSING
                mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                if (SetConsoleMode(h, mode)) return true;
            #endif
                return false;
            }

            // mintty, ConEmu...
            DWORD type = GetFileType(h);
            if (type == FILE_TYPE_PIPE) {
                char buf[128];
                DWORD r = GetEnvironmentVariableA("TERM", buf, 128);
                if (r != 0) {
                    // if r < 128, the buf is null terminated,
                    // r is not possible to be 128 in fact.
                    return r >= sizeof(buf) || strcmp(buf, "dumb") != 0;
                }
            }

            return false;
        }();
    #else
        g_color_enabled = !!isatty(fileno(stderr));
    #endif // ifdef _WIN32
        g_mtx = co::_make_rootic<std::mutex>();
    }
}

Print::Print()
    : _s(g_s ? *g_s : *(g_s = co::_make_rootic<stream>())) {
}

Print::~Print() {
    co::string& s = _s._s;
    if (s.size() >= 8000) {
        _s.flush();
        if (s.capacity() > 8192) s.swap(co::string(8192));
    }
}

PrintLn::PrintLn()
    : _s(g_sln ? *g_sln : *(g_sln = co::_make_rootic<stream>())) {
    _n = _s._s.size();
}

PrintLn::~PrintLn() {
    co::string& s = _s._s;
    s << '\n';
    write_to_console(s.data() + _n, s.size() - _n);
    s.resize(_n);
    if (_n == 0 && s.capacity() > 8192) s.swap(co::string(8192));
}

} // xx
} // co
