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
    char buf[4096];
    char* s = getcwd(buf, 4096);
    return s ? fastring(s) : fastring(1, '.');
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
    static int ncpu = (int) sysconf(_SC_NPROCESSORS_ONLN);
    return ncpu;
}

#ifdef __linux__
fastring exepath() {
    char buf[4096] = { 0 };
    int r = (int) readlink("/proc/self/exe", buf, 4096);
    return r > 0 ? fastring(buf, r) : fastring();
}

void daemon() {
    int r = ::daemon(1, 0); (void) r;
}

#else
fastring exepath() {
    char buf[4096] = { 0 };
    uint32_t size = sizeof(buf);
    int r = _NSGetExecutablePath(buf, &size);
    return r == 0 ? fastring(buf) : fastring();
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
    if (f == NULL) return false;
    return pclose(f) != -1;
}

} // os

#endif
