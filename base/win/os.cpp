#ifdef _WIN32

#include "../os.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace os {

fastring env(const char* name) {
    fastring s(256, '\0');
    DWORD r = GetEnvironmentVariableA(name, &s[0], 256);
    s.resize(r);

    if (r > 256) {
        GetEnvironmentVariableA(name, &s[0], r);
        s.resize(r - 1);
    }

    return s;
}

fastring homedir() {
    return os::env("SYSTEMDRIVE") + os::env("HOMEPATH");
}

fastring cwd() {
    char buf[264];
    DWORD r = GetCurrentDirectoryA(sizeof(buf), buf);
    return fastring(buf, r);
}

fastring exepath() {
    char buf[264]; // MAX_PATH = 260
    DWORD r = GetModuleFileNameA(0, buf, sizeof(buf));
    return fastring(buf, r);
}

fastring exename() {
    fastring s = exepath();
    return s.substr(s.rfind('\\') + 1);
}

int pid() {
    return (int) GetCurrentProcessId();
}

static inline int _Cpunum() {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (int) info.dwNumberOfProcessors;
}

int cpunum() {
    static int ncpu = _Cpunum();
    return ncpu;
}

void daemon() {}

} // os

#endif
