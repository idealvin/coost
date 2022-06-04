#include "co/error.h"
#include "co/fastream.h"
#include "co/stl.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace co {
namespace xx {

struct Error {
    Error() : e(0), s(4096) {}
    int e;
    fastream s;
    co::hash_map<int, uint32> pos;
};

inline Error& error() {
    static __thread Error* kE = 0;
    return kE ? *kE : *(kE = co::static_new<Error>());
}
} // xx

int& error() { return xx::error().e; }

const char* strerror(int e) {
    if (e == ETIMEDOUT || e == WSAETIMEDOUT) return "Timed out.";

    auto& err = xx::error();
    auto it = err.pos.find(e);
    if (it != err.pos.end()) return err.s.data() + it->second;

    uint32 pos = (uint32) err.s.size();
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
} // co

#else
#include <errno.h>
#include <string.h>
#include <mutex>

namespace co {
namespace xx {

struct Error {
    Error() : s(4096) {}
    fastream s;
    co::hash_map<int, uint32> pos;
};

inline Error& error() {
    static __thread Error* kE = 0;
    return kE ? *kE : *(kE = co::static_new<Error>());
}

} // xx

const char* strerror(int e) {
    if (e == ETIMEDOUT) return "Timed out";

    auto& err = xx::error();
    auto it = err.pos.find(e);
    if (it != err.pos.end()) return err.s.data() + it->second;

    uint32 pos = (uint32) err.s.size();
    static auto kMtx = co::static_new<std::mutex>();
    {
        std::lock_guard<std::mutex> g(*kMtx);
        err.s << ::strerror(e) << '\0';
    }

    err.pos[e] = pos;
    return err.s.data() + pos;
}
} // co

#endif
