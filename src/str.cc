#include "co/str.h"
#include <math.h>
#include <algorithm>

namespace str {

co::vector<fastring> split(const char* s, char c, uint32 maxsplit) {
    co::vector<fastring> v;
    v.reserve(8);

    const char* p;
    const char* from = s;

    while ((p = strchr(from, c))) {
        v.push_back(fastring(from, p - from));
        from = p + 1;
        if (v.size() == maxsplit) break;
    }

    if (from < s + strlen(s)) v.push_back(fastring(from));
    return v;
}

co::vector<fastring> split(const fastring& s, char c, uint32 maxsplit) {
    co::vector<fastring> v;
    v.reserve(8);

    const char* p;
    const char* from = s.data();
    const char* end = from + s.size();

    while ((p = (const char*) memchr(from, c, end - from))) {
        v.push_back(fastring(from, p - from));
        from = p + 1;
        if (v.size() == maxsplit) break;
    }

    if (from < end) v.push_back(fastring(from, end - from));
    return v;
}

co::vector<fastring> split(const char* s, const char* c, uint32 maxsplit) {
    co::vector<fastring> v;
    v.reserve(8);

    const char* p;
    const char* from = s;
    size_t n = strlen(c);

    while ((p = strstr(from, c))) {
        v.push_back(fastring(from, p - from));
        from = p + n;
        if (v.size() == maxsplit) break;
    }

    if (from < s + strlen(s)) v.push_back(fastring(from));
    return v;
}

fastring replace(const char* s, const char* sub, const char* to, uint32 maxreplace) {
    const char* p;
    const char* from = s;
    size_t n = strlen(sub);
    size_t m = strlen(to);

    fastring x;

    while ((p = strstr(from, sub))) {
        x.append(from, p - from);
        x.append(to, m);
        from = p + n;
        if (--maxreplace == 0) break;
    }

    if (from < s + strlen(s)) x.append(from);
    return x;
}

fastring replace(const fastring& s, const char* sub, const char* to, uint32 maxreplace) {
    const char* from = s.c_str();
    const char* p = strstr(from, sub);
    if (!p) return s;

    size_t n = strlen(sub);
    size_t m = strlen(to);
    fastring x(s.size());

    do {
        x.append(from, p - from).append(to, m);
        from = p + n;
        if (--maxreplace == 0) break;
    } while ((p = strstr(from, sub)));

    if (from < s.data() + s.size()) x.append(from);
    return x;
}

// co::error() is equal to errno on linux/mac, that's not the fact on windows.
#ifdef _WIN32
#define _co_set_error(e) co::error() = e
#define _co_reset_error() do { errno = 0; co::error() = 0; } while (0)
#else
#define _co_set_error(e)
#define _co_reset_error() errno = 0
#endif

bool to_bool(const char* s) {
    co::error() = 0;
    if (strcmp(s, "false") == 0 || strcmp(s, "0") == 0) return false;
    if (strcmp(s, "true") == 0 || strcmp(s, "1") == 0) return true;
    co::error() = EINVAL;
    return false;
}

int32 to_int32(const char* s) {
    int64 x = to_int64(s);
    if (unlikely(x > MAX_INT32 || x < MIN_INT32)) {
        co::error() = ERANGE;
        return 0;
    }
    return (int32)x;
}

uint32 to_uint32(const char* s) {
    int64 x = (int64) to_uint64(s);
    int64 absx = x < 0 ? -x : x;
    if (unlikely(absx > MAX_UINT32)) {
        co::error() = ERANGE;
        return 0;
    }
    return (uint32)x;
}

inline int _Shift(char c) {
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

int64 to_int64(const char* s) {
    _co_reset_error();
    if (!*s) return 0;

    char* end = 0;
    int64 x = strtoll(s, &end, 0);
    if (errno != 0) {
        _co_set_error(errno);
        return 0;
    }

    size_t n = strlen(s);
    if (end == s + n) return x;

    if (end == s + n - 1) {
        int shift = _Shift(s[n - 1]);
        if (shift != 0) {
            if (x == 0) return 0;
            if (x < (MIN_INT64 >> shift) || x > (MAX_INT64 >> shift)) {
                co::error() = ERANGE;
                return 0;
            }
            return x << shift;
        }
    }

    co::error() = EINVAL;
    return 0;
}

uint64 to_uint64(const char* s) {
    _co_reset_error();
    if (!*s) return 0;

    char* end = 0;
    uint64 x = strtoull(s, &end, 0);
    if (errno != 0) {
        _co_set_error(errno);
        return 0;
    }

    size_t n = strlen(s);
    if (end == s + n) return x;

    if (end == s + n - 1) {
        int shift = _Shift(s[n - 1]);
        if (shift != 0) {
            if (x == 0) return 0;
            int64 absx = (int64)x;
            if (absx < 0) absx = -absx;
            if (absx > static_cast<int64>(MAX_UINT64 >> shift)) {
                co::error() = ERANGE;
                return 0;
            }
            return x << shift;
        }
    }

    co::error() = EINVAL;
    return 0;
}

double to_double(const char* s) {
    _co_reset_error();
    char* end = 0;
    double x = strtod(s, &end);
    if (errno != 0) {
        _co_set_error(errno);
        return 0;
    }

    if (end == s + strlen(s)) return x;
    co::error() = EINVAL;
    return 0;
}

#undef _co_set_error
#undef _co_reset_error

} // namespace str
