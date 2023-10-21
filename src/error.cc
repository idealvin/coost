#include "co/error.h"
#include "co/fastream.h"
#include "co/stl.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace co {
namespace xx {

struct Error {
    Error() : s(4096) {}
    fastream s;
    co::hash_map<int, uint32> pos;
};

static __thread Error* g_err = 0;

inline Error& error() {
    return g_err ? *g_err : *(g_err = co::_make_static<Error>());
}

} // xx

#ifdef _WIN32
int error() { return ::GetLastError(); }
void error(int e) { ::SetLastError(e); }

const char* strerror(int e) {
    if (e == ETIMEDOUT || e == WSAETIMEDOUT) return "Timed out.";

    auto& err = xx::error();
    auto it = err.pos.find(e);
    if (it != err.pos.end()) return err.s.data() + it->second;

    const uint32 pos = (uint32) err.s.size();
    char* s = 0;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        0, e,
        MAKELANGID(LANG_ENGLISH /*LANG_NEUTRAL*/, SUBLANG_DEFAULT),
        (LPSTR)&s, 0, 0
    );

    if (s) {
        err.s << s << '\0';
        LocalFree(s);
        char* p = (char*) strchr(err.s.data() + pos, '\r');
        if (p) *p = '\0';
    } else {
        err.s << "Unknown error " << e << '\0';
    }

    err.pos[e] = pos;
    return err.s.data() + pos;
}

#else
const char* strerror(int e) {
    if (e == ETIMEDOUT) return "Timed out";

    auto& err = xx::error();
    auto it = err.pos.find(e);
    if (it != err.pos.end()) return err.s.data() + it->second;

    const uint32 pos = (uint32) err.s.size();
    char buf[256] = { 0 };
    auto r = ::strerror_r(e, buf, sizeof(buf));
    if (buf[0]) {
        err.s << buf << '\0';
    } else {
        err.s << r << '\0';
    }

    err.pos[e] = pos;
    return err.s.data() + pos;
}

#endif

} // co
