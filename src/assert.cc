#include "co/assert.h"
#include <stdio.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <stdlib.h>
#endif

namespace co {

int _strlen(const char* s) {
    int n = 0;
    while (*s++) ++n;
    return n;
}

const char* _fname(const char* s, int* n) {
    const char* const b = s;
    const char* p = s + _strlen(s);
    int i = 0;
    for (; p != b; --p) {
        const char c = *(p - 1);
        if (c == '/' || c == '\\') break;
        ++i;
    }
    *n = i;
    return p;
}

int _itos(int n, char* buf) {
    char* p = buf;
    char temp[12];
    int len = 0;

    while (n > 0) {
        temp[len++] = '0' + (n % 10);
        n /= 10;
    }

    for (int i = len - 1; i >= 0; --i) {
        *p++ = temp[i];
    }

    return (int)(p - buf);
}

void _assert_failed(const char* c, const char* file, int line, const char* e) {
    int n = 0;
    const char* p = _fname(file, &n);
    ::fwrite(p, 1, n, stderr);

    char buf[16];
    buf[0] = ':';
    const int len = _itos(line, buf + 1);
    buf[len + 1] = ']';
    buf[len + 2] = ' ';
    ::fwrite(buf, 1, len + 3, stderr);

    ::fwrite("runtime_assert(", 1, 15, stderr);
    ::fwrite(c, 1, _strlen(c), stderr);
    ::fwrite(") failed! ", 1, 10, stderr);

    int l = _strlen(e);
    if (l > 0) {
        if (*e == '"') {
            ::fwrite(e + 1, 1, l - 2, stderr);
        } else {
            ::fwrite(e, 1, l, stderr);
        }
    }
    ::fwrite("\n", 1, 1, stderr);

#ifdef _WIN32
    RaiseException(0xE880E233, 0, 0, NULL);
#else
    ::abort();
#endif
}

} // co
