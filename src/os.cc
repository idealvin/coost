#ifndef _WIN32

#include "co/os.h"
#include <stdio.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace os {

fastring env(const char* name) {
    char* x = ::getenv(name);
    return x ? fastring(x) : fastring();
}

bool env(const char* name, const char* value) {
    if (value == NULL || *value == '\0') return ::unsetenv(name) == 0;
    return ::setenv(name, value, 1) == 0;
}

fastring homedir() {
    return os::env("HOME");
}

fastring cwd() {
    fastring s(128);
    while (true) {
        if (::getcwd((char*)s.data(), s.capacity())) {
            s.resize(strlen(s.data()));
            return s;
        }
        if (errno != ERANGE) return fastring();
        s.reserve(s.capacity() << 1);
    }
}

fastring exedir() {
    fastring s = os::exepath();
    size_t n = s.rfind('/');
    if (n != s.npos) {
        if (n != 0) {
            s[n] = '\0';
            s.resize(n);
        } else {
            if (s.capacity() > 1) s[1] = '\0';
            s.resize(1);
        }
    }
    return s;
}

fastring exename() {
    fastring s = os::exepath();
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

#ifdef __linux__
fastring exepath() {
    fastring s(128);
    while (true) {
        auto r = readlink("/proc/self/exe", (char*)s.data(), s.capacity());
        if (r < 0) return fastring();
        if ((size_t)r != s.capacity()) {
            s.resize(r);
            return s;
        }
        s.reserve(s.capacity() << 1);
    }
}

void daemon() {
    const int r = ::daemon(1, 0); (void)r;
}

#else
fastring exepath() {
    fastring s(128);
    uint32_t n = 128;
    while (true) {
        if (_NSGetExecutablePath((char*)s.data(), &n) == 0) {
            s.resize(strlen(s.data()));
            return s;
        }

        // this is not likely to happen
        if (unlikely((size_t)n <= s.capacity())) return fastring();
        s.reserve(n);
    }
}

void daemon() {}
#endif

sig_handler_t signal(int sig, sig_handler_t handler, int flag) {
    struct sigaction sa, old;
    memset(&sa, 0, sizeof(sa));
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

#endif
