#include "co/string.h"
#include <limits.h> // for two_way.h

namespace co {

static bool _match(const char* s, size_t n, const char* p, size_t m) {
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

bool string::match(const char* pattern) const noexcept {
    return _match(_p, _size, pattern, strlen(pattern));
}

string& string::trim_left(char c) noexcept {
    size_t b = 0;
    while (b < _size && _p[b] == c) ++b;
    if (b != 0 && (_size -= b) != 0) ::memmove(_p, _p + b, _size);
    return *this;
}

string& string::trim_right(char c) noexcept {
    size_t e = _size;
    while (e > 0 && _p[e - 1] == c) --e;
    if (e != _size) _size = e;
    return *this;
}

string& string::trim(char c) noexcept {
    if (!this->empty()) {
        size_t b = 0, e = _size;
        while (e > 0 && _p[e - 1] == c) --e;
        if (e != _size) _size = e;
        while (b < _size && _p[b] == c) ++b;
        if (b != 0 && (_size -= b) != 0) ::memmove(_p, _p + b, _size);
    }
    return *this;
}

string& string::trim_left(const char* x) noexcept {
    if (!this->empty() && x && *x) {
        const unsigned char* s = (const unsigned char*)x;
        const unsigned char* const p = (const unsigned char*)_p;
        unsigned char bs[256] = { 0 };
        while (*s) bs[*s++] = 1;

        size_t b = 0;
        while (b < _size && bs[p[b]]) ++b;
        if (b != 0 && (_size -= b) != 0) ::memmove(_p, _p + b, _size);
    }
    return *this;
}

string& string::trim_right(const char* x) noexcept {
    if (!this->empty() && x && *x) {
        const unsigned char* s = (const unsigned char*)x;
        const unsigned char* const p = (const unsigned char*)_p;
        unsigned char bs[256] = { 0 };
        while (*s) bs[*s++] = 1;

        size_t e = _size;
        while (e > 0 && bs[p[e - 1]]) --e;
        if (e != _size) _size = e;
    }
    return *this;
}

string& string::trim(const char* x) noexcept {
    if (!this->empty() && x && *x) {
        const unsigned char* s = (const unsigned char*)x;
        const unsigned char* const p = (const unsigned char*)_p;
        unsigned char bs[256] = { 0 };
        while (*s) bs[*s++] = 1;

        size_t b = 0, e = _size;
        while (e > 0 && bs[p[e - 1]]) --e;
        if (e != _size) _size = e;
        while (b < _size && bs[p[b]]) ++b;
        if (b != 0 && (_size -= b) != 0) ::memmove(_p, _p + b, _size);
    }
    return *this;
}

string& string::remove_outer(size_t n) noexcept {
    if (!this->empty() && n > 0) {
        if (n < ((_size >> 1) + (_size & 1))) {
            _size -= n * 2;
            ::memmove(_p, _p + n, _size);
        } else {
            _size = 0;
        }
    }
    return *this;
}

string& string::remove_prefix(size_t n) noexcept {
    if (!this->empty() && n > 0) {
        if (n < _size) {
            _size -= n;
            ::memmove(_p, _p + n, _size);
        } else {
            _size = 0;
        }
    }
    return *this;
}

string& string::replace(const char* sub, size_t n, const char* to, size_t m, size_t maxreplace) noexcept {
    if (!this->empty() && n > 0) {
        const char* p = co::memmem(_p, _size, sub, n);
        if (p) {
            const char* from = _p;
            const char* const e = _p + _size;
            string s(_size + 1);
            do {
                s.append(from, p - from).append(to, m);
                from = p + n;
                if (maxreplace && --maxreplace == 0) break;
            } while ((p = co::memmem(from, e - from, sub, n)));

            if (from < _p + _size) s.append(from, e - from);
            this->swap(s);
        }
    }
    return *this;
}

string& string::escape() noexcept {
    const char* b = _p;
    const char* const e = _p + _size;
    string s;
    for (const char* p = b; p < e; ++p) {
        char c;
        switch (*p) {
            case '"':  c = '"';  break;
            case '\\': c = '\\'; break;
            case '\0': c = '0';  break;
            case '\r': c = 'r';  break;
            case '\n': c = 'n';  break;
            case '\t': c = 't';  break;
            case '\a': c = 'a';  break;
            case '\b': c = 'b';  break;
            case '\f': c = 'f';  break;
            case '\v': c = 'v';  break;
            default: continue;
        }
        if (s.empty()) s.reserve(s.size() + 8);
        s.append(b, p - b).append('\\').append(c);
        b = p + 1;
    }

    if (!s.empty()) {
        if (b < e) s.append(b, e - b);
        this->swap(s);
    }
    return *this;
}

string& string::unescape() noexcept {
    const char* b = _p;
    const char* const e = _p + _size;
    string s;
    for (const char* p = (char*)::memchr(b, '\\', e - b); p && p + 1 < e;) {
        char c;
        switch (*(p + 1)) {
            case '"':  c = '"';  break;
            case '\'': c = '\''; break;
            case '\\': c = '\\'; break;
            case '0':  c = '\0'; break;
            case 'r':  c = '\r'; break;
            case 'n':  c = '\n'; break;
            case 't':  c = '\t'; break;
            case 'a':  c = '\a'; break;
            case 'b':  c = '\b'; break;
            case 'f':  c = '\f'; break;
            case 'v':  c = '\v'; break;
            default:
                p = (char*)::memchr(p + 2, '\\', e - p - 2);
                continue;
        }
        if (s.empty()) s.reserve(s.size());
        s.append(b, p - b).append(c);
        b = p + 2;
        p = (char*)::memchr(b, '\\', e - b);
    }

    if (!s.empty()) {
        if (b < e) s.append(b, e - b);
        this->swap(s);
    }
    return *this;
}

string& string::toupper() noexcept {
    for (size_t i = 0; i < _size; ++i) {
        char& c = _p[i];
        if ('a' <= c && c <= 'z') c ^= 32;
    }
    return *this;
}

string& string::tolower() noexcept {
    for (size_t i = 0; i < _size; ++i) {
        char& c = _p[i];
        if ('A' <= c && c <= 'Z') c ^= 32;
    }
    return *this;
}

size_t string::find_first_of(const char* s, size_t pos, size_t n) const noexcept {
    if (pos < _size && n > 0) {
        unsigned char bs[256] = { 0 };
        for (size_t i = 0; i < n; ++i) bs[(unsigned char)s[i]] = 1;
        for (size_t i = pos; i < _size; ++i) {
            if (bs[(unsigned char)_p[i]]) return i;
        }
    }
    return npos;
}

size_t string::find_first_not_of(const char* s, size_t pos, size_t n) const noexcept {
    if (pos < _size) {
        unsigned char bs[256] = { 0 };
        for (size_t i = 0; i < n; ++i) bs[(unsigned char)s[i]] = 1;
        for (size_t i = pos; i < _size; ++i) {
            if (!bs[(unsigned char)_p[i]]) return i;
        }
    }
    return npos;
}

size_t string::find_first_not_of(char c, size_t pos) const noexcept {
    for (; pos < _size; ++pos) {
        if (_p[pos] != c) return pos;
    }
    return npos;
}

size_t string::find_last_of(const char* s, size_t pos, size_t n) const noexcept {
    if (_size > 0 && n > 0) {
        unsigned char bs[256] = { 0 };
        for (size_t i = 0; i < n; ++i) bs[(unsigned char)s[i]] = 1;
        for (size_t i = (pos >= _size ? _size : (pos + 1)); i > 0;) {
            if (bs[(unsigned char)_p[--i]]) return i;
        }
    }
    return npos;
}

size_t string::find_last_not_of(const char* s, size_t pos, size_t n) const noexcept {
    if (_size > 0) {
        unsigned char bs[256] = { 0 };
        for (size_t i = 0; i < n; ++i) bs[(unsigned char)s[i]] = 1;
        for (size_t i = (pos >= _size ? _size : (pos + 1)); i > 0;) {
            if (!bs[(unsigned char)_p[--i]]) return i;
        }
    }
    return npos;
}

size_t string::find_last_not_of(char c, size_t pos) const noexcept {
    if (_size > 0) {
        for (size_t i = (pos >= _size ? _size : (pos + 1)); i > 0;) {
            if (_p[--i] != c) return i;
        }
    }
    return npos;
}

void string::zero_clear() noexcept {
    const size_t l = _size;
    if (l > 0) {
        volatile size_t* s = (volatile size_t*)_p;
        const size_t n = (l / sizeof(size_t) ) + !!(l & (sizeof(size_t) - 1));
        for (size_t i = 0; i < n; ++i) s[i] = 0;
        _size = 0;
    }
}


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
        s = (const char*) ::memchr(s, *p, n);
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

    const char* const e = co::memrchr(s, *(p + m - 1), n);
    if (!e || m == 1) return (char*)e;
    if (static_cast<size_t>(e - s + 1) < m) return nullptr;

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


inline int _shift(char c) {
    switch (c) {
        case 'k':
        case 'K':
            return 10;
        case 'm':
        case 'M':
            return 20;
        case 'g':
        case 'G':
            return 30;
        case 't':
        case 'T':
            return 40;
        case 'p':
        case 'P':
            return 50;
        default:
            return 0;
    }
}

int32 stoi32(const char* s, int* e) noexcept {
    const int64 x = stoi64(s, e);
    if (co::min_int32 <= x && x <= co::max_int32)  return (int32)x;
    if (e) *e = ERANGE;
    return 0;
}

uint32 stou32(const char* s, int* e) noexcept {
    const int64 x = (int64) stou64(s, e);
    const int64 absx = x < 0 ? -x : x;
    if (absx <= co::max_uint32) return (uint32)x;
    if (e) *e = ERANGE;
    return 0;
}

int64 stoi64(const char* s, int* e) noexcept {
    errno = 0;
    if (e) *e = 0;

    char* end = 0;
    int64 x = ::strtoll(s, &end, 0);
    if (errno != 0) {
        if (e) *e = errno;
        return 0;
    }

    size_t n = strlen(s);
    if (end == s + n) return x;

    if (end == s + n - 1) {
        int shift = _shift(s[n - 1]);
        if (shift != 0) {
            if (x == 0) return 0;
            if (x < (co::min_int64 >> shift) || x > (co::max_int64 >> shift)) {
                if (e) *e = ERANGE;
                return 0;
            }
            return x << shift;
        }
    }

    if (e) *e = EINVAL;
    return 0;
}

uint64 stou64(const char* s, int* e) noexcept {
    errno = 0;
    if (e) *e = 0;

    char* end = 0;
    uint64 x = ::strtoull(s, &end, 0);
    if (errno != 0) {
        if (e) *e = errno;
        return 0;
    }

    size_t n = strlen(s);
    if (end == s + n) return x;

    if (end == s + n - 1) {
        int shift = _shift(s[n - 1]);
        if (shift != 0) {
            if (x == 0) return 0;
            int64 absx = (int64)x;
            if (absx < 0) absx = -absx;
            if (absx > static_cast<int64>(co::max_uint64 >> shift)) {
                if (e) *e = ERANGE;
                return 0;
            }
            return x << shift;
        }
    }

    if (e) *e = EINVAL;
    return 0;
}

bool stob(const char* s, int* e) noexcept {
    if (e) *e = 0;
    if (strcmp(s, "false") == 0 || strcmp(s, "0") == 0) return false;
    if (strcmp(s, "true") == 0 || strcmp(s, "1") == 0) return true;
    if (e) *e = EINVAL;
    return false;
}

double stod(const char* s, int* e) noexcept {
    errno = 0;
    if (e) *e = 0;
    char* end = 0;
    double x = ::strtod(s, &end);
    if (errno != 0) {
        if (e) *e = errno;
        return 0;
    }

    if (end == s + strlen(s)) return x;
    if (e) *e = EINVAL;
    return 0;
}


string replace(
    const char* s, size_t n,
    const char* sub, size_t m,
    const char* to, size_t l,
    size_t t
) noexcept {
    if (m == 0) return string(s, n);

    const char* p;
    const char* const end = s + n;
    string x(n);

    while ((p = co::memmem(s, end - s, sub, m))) {
        x.append(s, p - s).append(to, l);
        s = p + m;
        if (t && --t == 0) break;
    }

    if (s < end) x.append(s, end - s);
    return x;
}

vector<string> split(const char* s, size_t n, char c, size_t t) {
    vector<string> v;
    v.reserve(8);

    const char* p;
    const char* const end = s + n;

    while ((p = (const char*) ::memchr(s, c, end - s))) {
        v.emplace_back(s, p - s);
        s = p + 1;
        if (v.size() == t) break;
    }

    if (s < end) v.emplace_back(s, end - s);
    return v;
}

vector<string> split(const char* s, size_t n, const char* c, size_t m, size_t t) {
    vector<string> v;
    if (m == 0) return v;
    v.reserve(8);

    const char* p;
    const char* const end = s + n;

    while ((p = co::memmem(s, end - s, c, m))) {
        v.emplace_back(s, p - s);
        s = p + m;
        if (v.size() == t) break;
    }

    if (s < end) v.emplace_back(s, end - s);
    return v;
}

} // co
