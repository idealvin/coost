#include "co/fastring.h"

#include <limits.h>
#include <stdint.h>

namespace str {

inline bool _has_null(size_t x) {
    const size_t o = (size_t)-1 / 255;
    return (x - o) & ~x & (o * 0x80);
}

char* memrchr(const char* s, char c, size_t n) {
    if (n == 0) return nullptr;

    char* p = (char*)s + n - 1;
    while ((size_t)(p + 1) & (sizeof(size_t) - 1)) {
        if (*p == c) return p;
        if (p-- == s) return nullptr;
    }

    if (p - s >= sizeof(size_t) - 1) {
        const size_t mask = (size_t)-1 / 255 * (unsigned char)c;
        size_t* w = (size_t*)(p - (sizeof(size_t) - 1));
        do {
            if (_has_null(*w ^ mask)) break;
            --w;
        } while ((char*)w >= s);
        p = (char*)w + (sizeof(size_t) - 1);
    }

    while (p >= s) {
        if (*p == c) return p;
        --p;
    }
    return nullptr;
}

#define RETURN_TYPE void*
#define AVAILABLE(h, h_l, j, n_l) ((j) <= (h_l) - (n_l))
#include "two_way.h"

char* memmem(const char* s, size_t n, const char* p, size_t m) {
    if (n < m) return NULL;
    if (n == 0 || m == 0) return (char*)s;

    typedef unsigned char* S;
    if (m < LONG_NEEDLE_THRESHOLD) {
        const char* const b = s;
        s = (const char*) memchr(s, *p, n);
        if (!s || m == 1) return (char*)s;

        n -= s - b; 
        return n < m ? NULL : (char*)two_way_short_needle((S)s, n, (S)p, m);
    }

    return (char*)two_way_long_needle((S)s, n, (S)p, m);
}

static int _memicmp(const void* s, const void* t, size_t n) {
    const unsigned char* p = (const unsigned char*)s;
    const unsigned char* q = (const unsigned char*)t;
    int d = 0;
    for (; n != 0; --n) {
        if ((d = ::tolower(*p++) - ::tolower(*q++)) != 0) break;
    }
    return d;
}

#define RETURN_TYPE void*
#define AVAILABLE(h, h_l, j, n_l) ((j) <= (h_l) - (n_l))
#define FN_NAME(x) x##_i
#define CANON_ELEMENT(c) ::tolower(c)
#define CMP_FUNC _memicmp
#include "two_way.h"

char* memimem(const char* s, size_t n, const char* p, size_t m) {
    if (n < m) return NULL;
    if (n == 0 || m == 0) return (char*)s;

    typedef unsigned char* S;
    if (m < LONG_NEEDLE_THRESHOLD) {
        return (char*)two_way_short_needle_i((S)s, n, (S)p, m);
    }
    return (char*)two_way_long_needle_i((S)s, n, (S)p, m);
}

char* memrmem(const char* s, size_t n, const char* p, size_t m) {
    if (n < m) return nullptr;
    if (m == 0) return (char*)(s + n);

    const char* const e = str::memrchr(s, *(p + m - 1), n);
    if (!e || m == 1) return (char*)e;
    if (e - s + 1 < m) return nullptr;

    size_t off[256] = { 0 };
    for (size_t i = m; i > 0; --i) off[(unsigned char)p[i - 1]] = i;

    for (const char* b = e - m + 1;;) {
        if (::memcmp(b, p, m) == 0) return (char*)b;
        if (b == s) return nullptr;

        size_t o = off[(unsigned char)*(b - 1)];
        if (o == 0) o = m + 1;
        if (b < s + o) return nullptr;
        b -= o;
    }
}

bool match(const char* s, size_t n, const char* p, size_t m) {
    char c;
    while (n > 0 && m > 0 && (c = p[m - 1]) != '*') {
        if (c != s[n - 1] && c != '?') return false;
        --n, --m;
    }
    if (m == 0) return n == 0;

    size_t si = 0, pi = 0, sl = -1, pl = -1;
    while (si < n && pi < m) {
        c = p[pi];
        if (c == '*') {
            sl = si;
            pl = ++pi;
            continue;
        }

        if (c == s[si] || c == '?') {
            ++si, ++pi;
            continue;
        }

        if (sl != (size_t)-1 && sl + 1 < n) {
            si = ++sl;
            pi = pl;
            continue;
        }

        return false;
    }

    while (pi < m) {
        if (p[pi++] != '*') return false;
    }
    return true;
}

} // str

fastring& fastring::trim(char c, char d) {
    if (this->empty()) return *this;

    size_t b, e;
    switch (d) {
      case 'r':
      case 'R':
        e = _size;
        while (e > 0 && _p[e - 1] == c) --e;
        if (e != _size) _size = e;
        break;
      case 'l':
      case 'L':
        b = 0;
        while (b < _size && _p[b] == c) ++b;
        if (b != 0 && (_size -= b) != 0) memmove(_p, _p + b, _size);
        break;
      default:
        b = 0, e = _size;
        while (e > 0 && _p[e - 1] == c) --e;
        if (e != _size) _size = e;
        while (b < _size && _p[b] == c) ++b;
        if (b != 0 && (_size -= b) != 0) memmove(_p, _p + b, _size);
        break;
    }

    return *this;

}

fastring& fastring::trim(const char* x, char d) {
    if (this->empty() || !x || !*x) return *this;

    const unsigned char* s = (const unsigned char*)x;
    const unsigned char* const p = (const unsigned char*)_p;
    unsigned char bs[256] = { 0 };
    while (*s) bs[*s++] = 1;

    size_t b, e;
    switch (d) {
      case 'r':
      case 'R':
        e = _size;
        while (e > 0 && bs[p[e - 1]]) --e;
        if (e != _size) _size = e;
        break;
      case 'l':
      case 'L':
        b = 0;
        while (b < _size && bs[p[b]]) ++b;
        if (b != 0 && (_size -= b) != 0) memmove(_p, _p + b, _size);
        break;
      default:
        b = 0, e = _size;
        while (e > 0 && bs[p[e - 1]]) --e;
        if (e != _size) _size = e;
        while (b < _size && bs[p[b]]) ++b;
        if (b != 0 && (_size -= b) != 0) memmove(_p, _p + b, _size);
        break;
    }

    return *this;
}

fastring& fastring::trim(size_t n, char d) {
    if (!this->empty()) {
        switch (d) {
          case 'r':
          case 'R':
            _size = n < _size ? _size - n : 0;
            break;
          case 'l':
          case 'L':
            if (n < _size) {
                _size -= n;
                memmove(_p, _p + n, _size);
            } else {
                _size = 0;
            }
            break;
          default:
            if (n * 2 < _size) {
                _size -= n * 2;
                memmove(_p, _p + n, _size);
            } else {
                _size = 0;
            }
        }
    }
    return *this;
}

fastring& fastring::replace(const char* sub, size_t n, const char* to, size_t m, size_t maxreplace) {
    if (this->empty() || n == 0) return *this;

    const char* from = _p;
    const char* p = str::memmem(_p, _size, sub, n);
    if (!p) return *this;

    const char* const e = _p + _size;
    fastring s(_size + 1);

    do {
        s.append(from, p - from).append(to, m);
        from = p + n;
        if (maxreplace && --maxreplace == 0) break;
    } while ((p = str::memmem(from, e - from, sub, n)));

    if (from < _p + _size) s.append(from, e - from);

    this->swap(s);
    return *this;
}

fastring& fastring::toupper() {
    for (size_t i = 0; i < _size; ++i) {
        char& c = _p[i];
        if ('a' <= c && c <= 'z') c ^= 32;
    }
    return *this;
}

fastring& fastring::tolower() {
    for (size_t i = 0; i < _size; ++i) {
        char& c = _p[i];
        if ('A' <= c && c <= 'Z') c ^= 32;
    }
    return *this;
}

size_t fastring::find_first_of(const char* s, size_t pos, size_t n) const {
    if (pos < _size && n > 0) {
        unsigned char bs[256] = { 0 };
        for (size_t i = 0; i < n; ++i) bs[(unsigned char)s[i]] = 1;
        for (size_t i = pos; i < _size; ++i) {
            if (bs[(unsigned char)_p[i]]) return i;
        }
    }
    return npos;
}

size_t fastring::find_first_not_of(const char* s, size_t pos, size_t n) const {
    if (pos < _size) {
        unsigned char bs[256] = { 0 };
        for (size_t i = 0; i < n; ++i) bs[(unsigned char)s[i]] = 1;
        for (size_t i = pos; i < _size; ++i) {
            if (!bs[(unsigned char)_p[i]]) return i;
        }
    }
    return npos;
}

size_t fastring::find_first_not_of(char c, size_t pos) const {
    for (; pos < _size; ++pos) {
        if (_p[pos] != c) return pos;
    }
    return npos;
}

size_t fastring::find_last_of(const char* s, size_t pos, size_t n) const {
    if (_size > 0 && n > 0) {
        unsigned char bs[256] = { 0 };
        for (size_t i = 0; i < n; ++i) bs[(unsigned char)s[i]] = 1;
        for (size_t i = (pos >= _size ? _size : (pos + 1)); i > 0;) {
            if (bs[(unsigned char)_p[--i]]) return i;
        }
    }
    return npos;
}

size_t fastring::find_last_not_of(const char* s, size_t pos, size_t n) const {
    if (_size > 0) {
        unsigned char bs[256] = { 0 };
        for (size_t i = 0; i < n; ++i) bs[(unsigned char)s[i]] = 1;
        for (size_t i = (pos >= _size ? _size : (pos + 1)); i > 0;) {
            if (!bs[(unsigned char)_p[--i]]) return i;
        }
    }
    return npos;
}

size_t fastring::find_last_not_of(char c, size_t pos) const {
    if (_size > 0) {
        for (size_t i = (pos >= _size ? _size : (pos + 1)); i > 0;) {
            if (_p[--i] != c) return i;
        }
    }
    return npos;
}
