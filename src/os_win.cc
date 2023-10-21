#ifdef _WIN32

#include "co/os.h"
#include <signal.h>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace os {

fastring env(const char* name) {
    fastring s(64);
    DWORD r = GetEnvironmentVariableA(name, (char*)s.data(), 64);
    s.resize(r);
    if (r > 64) {
        GetEnvironmentVariableA(name, (char*)s.data(), r);
        s.resize(r - 1);
    }
    return s;
}

bool env(const char* name, const char* value) {
    return SetEnvironmentVariableA(name, value) == TRUE;
}

inline void backslash_to_slash(fastring& s) {
    std::for_each((char*)s.data(), (char*)s.data() + s.size(), [](char& c){
        if (c == '\\') c = '/';
    });
}

fastring homedir() {
    fastring s = os::env("USERPROFILE"); // SYSTEMDRIVE + HOMEPATH
    backslash_to_slash(s);
    return s;
}

fastring cwd() {
    fastring s(64);
    DWORD r = GetCurrentDirectoryA(64, (char*)s.data());
    s.resize(r);
    if (r > 64) {
        GetCurrentDirectoryA(r, (char*)s.data());
        s.resize(r - 1);
    }
    if (!(s.size() > 1 && s[0] == '\\' && s[1] == '\\')) backslash_to_slash(s);
    return s;
}

static fastring _get_module_path() {
    DWORD n = 128, r = 0;
    fastring s(128);
    while (true) {
        r = GetModuleFileNameA(NULL, (char*)s.data(), n);
        if (r < n) { s.resize(r); break; }
        n <<= 1;
        s.reserve(n);
    }
    return s;
}

fastring exepath() {
    fastring s = _get_module_path();
    if (!(s.size() > 1 && s[0] == '\\' && s[1] == '\\')) backslash_to_slash(s);
    return s;
}

fastring exedir() {
    fastring s = _get_module_path();
    size_t n = s.rfind('\\');
    if (n != s.npos && n != 0) {
        if (s[n - 1] != ':') {
            s[n] = '\0';
            s.resize(n);
        } else {
            s.resize(n + 1);
            if (s.capacity() > n + 1) s[n + 1] = '\0';
        }
    }

    if (!(s.size() > 1 && s[0] == '\\' && s[1] == '\\')) backslash_to_slash(s);
    return s;
}

fastring exename() {
    fastring s = _get_module_path();
    return s.substr(s.rfind('\\') + 1);
}

int pid() {
    return (int) GetCurrentProcessId();
}

int cpunum() {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (int) info.dwNumberOfProcessors;
}

size_t pagesize() {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (size_t) info.dwPageSize;
}

void daemon() {}

sig_handler_t signal(int sig, sig_handler_t handler, int) {
    return ::signal(sig, handler);
}

bool system(const char* cmd) {
    return ::system(cmd) != -1;
}

} // os

#endif
