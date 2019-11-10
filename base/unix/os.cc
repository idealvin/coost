#ifndef _WIN32

#include "../os.h"
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace os {

fastring env(const char* name) {
    char* x = getenv(name);
    return x ? fastring(x) : fastring();
}

fastring homedir() {
    return os::env("HOME");
}

fastring cwd() {
    char buf[4096];
    char* s = getcwd(buf, 4096);
    return  s ? fastring(s) : fastring(1, '.');
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

} // os

#endif
