#include "co/fastring.h"

fastring& fastring::trim(char c, char d) {
    if (this->empty()) return *this;

    size_t b, e;
    switch (d) {
      case 'r':
      case 'R':
        e = _size;
        while (e > 0 && _p[e - 1] == c) --e;
        e != _size ? (void)(_size = e) : (void)0;
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
        e != _size ? (void)(_size = e) : (void)0;
        while (b < _size && _p[b] == c) ++b;
        if (b != 0 && (_size -= b) != 0) memmove(_p, _p + b, _size);
        break;
    }

    return *this;

}

fastring& fastring::trim(const char* x, char d) {
    if (this->empty() || !x || !*x) return *this;

    const unsigned char* s = (const unsigned char*)x;
    const unsigned char* p = (const unsigned char*)_p;
    unsigned char bs[256] = { 0 };
    while (*s) bs[*s++] = 1;

    size_t b, e;
    switch (d) {
      case 'r':
      case 'R':
        e = _size;
        while (e > 0 && bs[p[e - 1]]) --e;
        e != _size ? (void)(_size = e) : (void)0;
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
        e != _size ? (void)(_size = e) : (void)0;
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

fastring& fastring::replace(const char* sub, const char* to, size_t maxreplace) {
    if (this->empty()) return *this;

    const char* from = this->c_str();
    const char* p = strstr(from, sub);
    if (!p) return *this;

    const size_t n = strlen(sub);
    const size_t m = strlen(to);

    fastring s(_size);

    do {
        s.append(from, p - from).append(to, m);
        from = p + n;
        if (maxreplace && --maxreplace == 0) break;
    } while ((p = strstr(from, sub)));

    if (from < _p + _size) s.append(from);

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

size_t fastring::rfind(const char* sub) const {
    const size_t m = strlen(sub);
    if (m == 1) return this->rfind(*sub);

    const size_t n = this->size();
    if (n < m) return npos;

    const unsigned char* const p = (const unsigned char*) _p;
    const unsigned char* const s = (const unsigned char*) sub;

    size_t tbl[256] = { 0 };
    for (size_t i = m; i > 0; --i) tbl[s[i - 1]] = i;

    for (size_t j = n - m;;) {
        if (memcmp(s, p + j, m) == 0) return j;
        if (j == 0) return npos;

        size_t x = tbl[p[j - 1]];
        if (x == 0) x = m + 1;
        if (j < x) return npos;
        j -= x;
    }
}

size_t fastring::find_first_not_of(char c, size_t pos) const {
    for (; pos < _size; ++pos) {
        if (_p[pos] != c) return pos;
    }
    return npos;
}

size_t fastring::find_last_of(const char* x, size_t pos) const {
    if (!this->empty()) {
        auto s = (const unsigned char*)x;
        const auto p = (const unsigned char*)_p;
        unsigned char bs[256] = { 0 };
        while (*s) bs[*s++] = 1;

        for (size_t i = (pos >= _size ? _size : (pos + 1)); i > 0;) {
            if (bs[p[--i]]) return i;
        }
    }
    return npos;
}

size_t fastring::find_last_not_of(const char* x, size_t pos) const {
    if (!this->empty()) {
        auto s = (const unsigned char*)x;
        const auto p = (const unsigned char*)_p;
        unsigned char bs[256] = { 0 };
        while (*s) bs[*s++] = 1;

        for (size_t i = (pos >= _size ? _size : (pos + 1)); i > 0;) {
            if (!bs[p[--i]]) return i;
        }
    }
    return npos;
}

size_t fastring::find_last_not_of(char c, size_t pos) const {
    if (!this->empty()) {
        for (size_t i = (pos >= _size ? _size : (pos + 1)); i > 0;) {
            if (_p[--i] != c) return i;
        }
    }
    return npos;
}

static bool _match(const char* e, const char* p) {
    if (*p == '*' && !p[1]) return true;

    for (; *p && *e;) {
        if (*p == '*') return _match(e, p + 1) || _match(e + 1, p);
        if (*p != '?' && *p != *e) return false;
        ++p, ++e;
    }

    return (*p == '*' && !p[1]) || (!*p && !*e);
}

bool fastring::match(const char* p) const {
    return _match(this->c_str(), p);
}
