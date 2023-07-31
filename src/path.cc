#include "co/path.h"
#include "ctype.h"

namespace path {

fastring clean(const char* s, size_t n) {
    if (n == 0) return fastring(1, '.');

    fastring r(s, n);
    bool rooted = s[0] == '/';
    size_t beg = rooted;
    size_t p = rooted;      // index for string r
    size_t dotdot = rooted; // index where .. must stop

  #ifdef _WIN32
    if (!rooted && n > 2 && s[1] == ':' && s[2] == '/' && ::isalpha(s[0])) {
        rooted = true;
        beg = p = dotdot = 3;
    }
  #endif

    for (size_t i = p; i < n;) {
        if (s[i] == '/' || (s[i] == '.' && (i+1 == n || s[i+1] == '/'))) {
            // empty or . element
            ++i;

        } else if (s[i] == '.' && s[i+1] == '.' && (i+2 == n || s[i+2] == '/')) {
            // .. element: remove to last /
            i += 2;

            if (p > dotdot) {
                // backtrack
                for (--p; p > dotdot && r[p] != '/'; --p);

            } else if (!rooted) {
                // cannot backtrack, but not rooted, so append .. element
                if (p > 0) r[p++] = '/';
                r[p++] = '.';
                r[p++] = '.';
                dotdot = p;
            }

        } else {
            // real path element, add slash if needed
            if ((rooted && p != beg) || (!rooted && p != 0)) {
                r[p++] = '/';
            }

            // copy element until the next /
            for (; i < n && s[i] != '/'; ++i) {
                r[p++] = s[i];
            }
        }
    }

    if (p == 0) return fastring(1, '.');
    if (p < r.size()) r.resize(p);
    return r;
}

std::pair<fastring, fastring> split(const char* s, size_t n) {
  #ifdef _WIN32
    if (n == 2 && s[1] == ':' && ::isalpha(s[0])) {
        return std::make_pair(fastring(s, n), fastring());
    }
  #endif
 
    const char* p = str::memrchr(s, '/', n) + 1;
    if (p != (char*)1) {
        const size_t m = p - s;
        return std::make_pair(fastring(s, m), fastring(p, n - m));
    }
    return std::make_pair(fastring(), fastring(s, n));
}

fastring dir(const char* s, size_t n) {
  #ifdef _WIN32
    if (n == 2 && s[1] == ':' && ::isalpha(s[0])) {
        return fastring(s, n);
    }
  #endif
 
    const char* p = str::memrchr(s, '/', n);
    return p ? clean(fastring(s, p + 1 - s)): fastring(1, '.');
}

fastring base(const char* s, size_t n) {
    if (n == 0) return fastring(1, '.');

    size_t p = n;
    for (; p > 0; --p) {
        if (s[p - 1] != '/') break;
    }
    if (p == 0) return fastring(1, '/');
  #ifdef _WIN32
    if (p == 2 && s[1] == ':' && ::isalpha(s[0])) {
        return fastring(s, n > 3 ? 3 : n);
    }
  #endif

    size_t e = p;
    for (; p > 0; --p) {
        if (s[p - 1] == '/') break;
    }
    return fastring(s + p, e - p);
}

fastring ext(const char* s, size_t n) {
    const char* const e = s + n;
    for (const char* p = e; p != s && *--p != '/';) {
        if (*p == '.') return fastring(p, e - p);
    }
    return fastring();
}

} // namespace path
