#include "path.h"

namespace path {

fastring clean(const fastring& s) {
    if (s.empty()) return fastring(1, '.');

    fastring r(s.data(), s.size());
    size_t n = s.size();

    bool rooted = s[0] == '/';
    size_t p = rooted ? 1 : 0;      // index for string r
    size_t dotdot = rooted ? 1 : 0; // index where .. must stop

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
            if ((rooted && p != 1) || (!rooted && p != 0)) {
                r[p++] = '/';
            }

            // copy element until the next /
            for (; i < n && s[i] != '/'; i++) {
                r[p++] = s[i];
            }
        }
    }

    if (p == 0) return fastring(1, '.');
    return r.substr(0, p);
}

fastring base(const fastring& s) {
    if (s.empty()) return fastring(1, '.');

    size_t p = s.size();
    for (; p > 0; p--) {
        if (s[p - 1] != '/') break;
    }
    if (p == 0) return fastring(1, '/');

    fastring x = (p == s.size() ? s : s.substr(0, p));
    size_t c = x.rfind('/');
    return c != x.npos ? x.substr(c + 1) : x;
}

fastring ext(const fastring& s) {
    for (size_t i = s.size() - 1; i != (size_t)-1 && s[i] != '/'; --i) {
        if (s[i] == '.') return s.substr(i);
    }
    return fastring();
}

} // namespace path
