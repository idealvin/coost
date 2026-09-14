#include "co/error.h"
#include "co/string.h"
#include "co/stl.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace co {

struct ErrorMsg {
    ErrorMsg() : s(4096) {}
    co::string s;
    co::hash_map<int, uint32> pos;
};

static __thread ErrorMsg* g_error_msg = 0;

inline ErrorMsg& error_msg() {
    const auto p = g_error_msg;
    return p ? *p : *(g_error_msg = co::_make_rootic<ErrorMsg>());
}

#ifdef _WIN32
int error() { return ::GetLastError(); }
void error(int e) { ::SetLastError(e); }

const char* strerror(int e) {
    if (e == ETIMEDOUT || e == WSAETIMEDOUT) return "Timed out.";

    auto& em = error_msg();
    auto it = em.pos.find(e);
    if (it != em.pos.end()) return em.s.data() + it->second;

    const uint32 pos = (uint32) em.s.size();
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
        em.s << s << '\0';
        LocalFree(s);
        char* p = (char*) ::strchr(em.s.data() + pos, '\r');
        if (p) *p = '\0';
    } else {
        em.s << "Unknown error " << e << '\0';
    }

    em.pos[e] = pos;
    return em.s.data() + pos;
}

#else
const char* strerror(int e) {
    if (e == ETIMEDOUT) return "Timed out";

    auto& em = error_msg();
    auto it = em.pos.find(e);
    if (it != em.pos.end()) return em.s.data() + it->second;

    const uint32 pos = (uint32) em.s.size();
    char buf[256] = { 0 };
    auto r = ::strerror_r(e, buf, sizeof(buf));
    if (buf[0]) {
        em.s << buf << '\0';
    } else {
        em.s << r << '\0';
    }

    em.pos[e] = pos;
    return em.s.data() + pos;
}

#endif

} // co
