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

char* _strcat(char* dst, const char* src, int n) {
    for (size_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
    return dst + n;
}

void _assert_failed(const char* c, const char* file, int line, const char* e) {
    int fn_len = 0;
    const char* fn = _fname(file, &fn_len);

    char line_buf[12];
    int line_len = _itos(line, line_buf);
    int c_len = _strlen(c);

    int e_len = _strlen(e);
    if (e_len > 0 && *e == '"') {
        ++e;
        e_len -= 2;
        if (e_len < 0) e_len = 0;
    }

    // layout:
    //   <file> ':' <line> "] runtime_assert(" <cond> ") failed! " <desc> '\n'
    //   fn_len  1  line_len         17         c_len      10       e_len  1
    const int total = fn_len + 1 + line_len + 17 + c_len + 10 + e_len + 1;
    char buf[512];

    if (total <= (int)sizeof(buf)) {
        char* p = buf;
        p = _strcat(p, fn, fn_len);
        *p++ = ':';
        p = _strcat(p, line_buf, line_len);
        p = _strcat(p, "] runtime_assert(", 17);
        p = _strcat(p, c, c_len);
        p = _strcat(p, ") failed! ", 10);
        if (e_len > 0) p = _strcat(p, e, e_len);
        *p++ = '\n';

        // single fwrite
        ::fwrite(buf, 1, (size_t)(p - buf), stderr);

    } else {
        // extremely long expression / description: fallback to segmented writes
        ::fwrite(fn, 1, (size_t)fn_len, stderr);
        ::fwrite(":", 1, 1, stderr);
        ::fwrite(line_buf, 1, (size_t)line_len, stderr);
        ::fwrite("] runtime_assert(", 1, 17, stderr);
        ::fwrite(c, 1, (size_t)c_len, stderr);
        ::fwrite(") failed! ", 1, 10, stderr);
        if (e_len > 0) ::fwrite(e, 1, (size_t)e_len, stderr);
        ::fwrite("\n", 1, 1, stderr);
    }

#ifdef _WIN32
    RaiseException(0xE880E233, 0, 0, NULL);
#else
    ::abort();
#endif
}

} // co
