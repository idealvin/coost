#include "co/os.h"

#ifndef _WIN32
#include <stdio.h>       // popen, pclose
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h> // _NSGetExecutablePath
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

namespace os {

co::string env(const char* name) {
    char* x = ::getenv(name);
    return x ? co::string(x) : co::string();
}

bool env(const char* name, const char* value) {
    if (value && *value) return ::setenv(name, value, 1) == 0;
    return ::unsetenv(name) == 0;
}

co::string homedir() {
    return os::env("HOME");
}

co::string cwd() {
    co::string s(128);
    while (true) {
        if (::getcwd(s.data(), s.capacity())) {
            s.resize(strlen(s.data()));
            return s;
        }
        if (errno != ERANGE) return co::string();
        s.reserve(s.capacity() << 1);
    }
}

#ifdef __APPLE__
co::string exepath() {
    co::string s(128);
    uint32_t n = 128;
    while (true) {
        if (_NSGetExecutablePath(s.data(), &n) == 0) {
            s.resize(strlen(s.data()));
            return s;
        }
        s.reserve(n); // n contains '\0'
    }
}

#elif defined(__FreeBSD__) || defined(__DragonFly__)
co::string exepath() {
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
    co::string s(128);
    size_t n = 128;
    while (true) {
        if (sysctl(mib, 4, s.data(), &n, NULL, 0) == 0) {
            s.resize(strlen(s.data()));
            return s;
        }
        if (errno != ENOMEM) return co::string();
        s.reserve(n); // FreeBSD updates n to needed size on ENOMEM
    }
}

#else
co::string exepath() {
    co::string s(128);
    while (true) {
        auto r = readlink("/proc/self/exe", s.data(), s.capacity());
        if (r < 0) return co::string();
        if ((size_t)r != s.capacity()) {
            s.resize(r);
            return s;
        }
        s.reserve(s.capacity() << 1);
    }
}
#endif

co::string exedir() {
    co::string s = os::exepath();
    size_t n = s.rfind('/');
    if (n != s.npos) {
        if (n != 0) {
            s[n] = '\0';
            s.resize(n);
        } else {
            s[1] = '\0';
            s.resize(1);
        }
    }
    return s;
}

co::string exename() {
    co::string s = os::exepath();
    return s.substr(s.rfind('/') + 1);
}

int pid() {
    return (int) getpid();
}

int cpunum() {
    return (int) sysconf(_SC_NPROCESSORS_ONLN);
}

size_t pagesize() {
    return (size_t) sysconf(_SC_PAGESIZE);
}

sig_handler_t signal(int sig, sig_handler_t handler, int flag) {
    struct sigaction sa, old;
    ::memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    if (flag > 0) sa.sa_flags = flag;
    sa.sa_handler = handler;
    int r = sigaction(sig, &sa, &old);
    return r == 0 ? old.sa_handler : SIG_ERR;
}

bool system(const char* cmd) {
    FILE* f = popen(cmd, "w");
    return f ? pclose(f) != -1 : false;
}

} // os

#else
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace os {

static co::string wchar2utf8(const wchar_t* p) {
    co::string s;
    int n = WideCharToMultiByte(CP_UTF8, 0, p, -1, NULL, 0, NULL, NULL);
    if (n > 0) {
        s.reserve(n);
        WideCharToMultiByte(CP_UTF8, 0, p, -1, s.data(), n, NULL, NULL);
        s.resize(n - 1);
    }
    return s;
}

co::string env(const char* name) {
    co::string s(64);
    DWORD r = GetEnvironmentVariableA(name, s.data(), 64);
    s.resize(r);
    if (r > 64) {
        GetEnvironmentVariableA(name, s.data(), r);
        s.resize(r - 1);
    }
    return s;
}

bool env(const char* name, const char* value) {
    return SetEnvironmentVariableA(name, value) == TRUE;
}

inline void backslash_to_slash(co::string& s) {
    std::for_each(s.data(), s.data() + s.size(), [](char& c){
        if (c == '\\') c = '/';
    });
}

co::string homedir() {
    co::string s = os::env("USERPROFILE"); // SYSTEMDRIVE + HOMEPATH
    backslash_to_slash(s);
    return s;
}

co::string cwd() {
    co::vector<wchar_t> v;
    v.reserve(128);
    *v.data() = 0;

    DWORD r = GetCurrentDirectoryW(128, v.data());
    if (r > 128) {
        v.reserve(r);
        GetCurrentDirectoryW(r, v.data());
    }

    co::string s = wchar2utf8(v.data());
    if (!s.starts_with("\\\\")) backslash_to_slash(s);
    return s;
}

static co::string _get_module_path() {
    DWORD n = 128, r = 0;
    co::vector<wchar_t> v;
    v.reserve(n);
    *v.data() = 0;

    while (true) {
        r = GetModuleFileNameW(NULL, v.data(), n);
        if (r < n) break;
        n <<= 1;
        v.reserve(n);
    }

    return wchar2utf8(v.data());
}

co::string exepath() {
    co::string s = _get_module_path();
    if (!s.starts_with("\\\\")) backslash_to_slash(s);
    return s;
}

co::string exedir() {
    co::string s = _get_module_path();
    size_t n = s.rfind('\\');
    if (n != s.npos && n != 0) {
        if (s[n - 1] != ':') {
            s[n] = '\0';
            s.resize(n);
        } else {
            s.resize(n + 1);
            s[n + 1] = '\0';
        }
    }

    if (!s.starts_with("\\\\")) backslash_to_slash(s);
    return s;
}

co::string exename() {
    co::string s = _get_module_path();
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

sig_handler_t signal(int sig, sig_handler_t handler, int) {
    return ::signal(sig, handler);
}

bool system(const char* cmd) {
    return ::system(cmd) != -1;
}

} // os

#endif
