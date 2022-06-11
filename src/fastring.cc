#include "co/fastring.h"

fastring& fastring::operator=(const char* s) {
    if (!*s) { this->clear(); return *this; }

    if (!this->_Inside(s)) {
        _size = strlen(s);
        this->reserve(_size + 1);
        memcpy(_p, s, _size + 1);
    } else if (s != _p) {
        _size -= (s - _p);
        memmove(_p, s, _size + 1);
    }

    return *this;
}

fastring& fastring::append(const void* x, size_t n) {
    const char* p = (const char*) x;

    if (!this->_Inside(p)) {
        return (fastring&) fast::stream::append(p, n);
    } else {
        assert(p + n <= _p + _size);
        size_t pos = p - _p;
        this->ensure(n);
        memcpy(_p + _size, _p + pos, n);
        _size += n;
        return *this;
    }
}

fastring& fastring::append(const fastring& s) {
    if (&s != this) {
        if (s.empty()) return *this;
        return (fastring&) fast::stream::append(s.data(), s.size());
    } else { /* append itself */
        if (_p) {
            this->reserve(_size << 1);
            memcpy(_p + _size, _p, _size);
            _size <<= 1;
        }
        return *this;
    }
}

size_t fastring::rfind(const char* sub) const {
    size_t m = strlen(sub);
    if (m == 1) return this->rfind(*sub);

    size_t n = this->size();
    if (n < m) return npos;

    const unsigned char* s = (const unsigned char*) _p;
    const unsigned char* p = (const unsigned char*) sub;

    size_t tbl[256] = { 0 };
    for (size_t i = m; i > 0; --i) tbl[p[i - 1]] = i;

    for (size_t j = n - m;;) {
        if (memcmp(p, s + j, m) == 0) return j;
        if (j == 0) return npos;

        size_t x = tbl[s[j - 1]];
        if (x == 0) x = m + 1;
        if (j < x) return npos;
        j -= x;
    }
}

size_t fastring::find_last_of(const char* s, size_t pos) const {
    if (this->empty()) return npos;

    typedef unsigned char u8;
    char bs[256] = { 0 };
    while (*s) bs[(const u8) (*s++)] = 1;

    size_t i = (pos >= _size ? _size : (pos + 1));
    for (; i > 0;) {
        if (bs[(u8) (_p[--i])]) return i;
    }

    return npos;
}

size_t fastring::find_last_not_of(const char* s, size_t pos) const {
    if (this->empty()) return npos;

    typedef unsigned char u8;
    char bs[256] = { 0 };
    while (*s) bs[(const u8) (*s++)] = 1;

    size_t i = (pos >= _size ? _size : (pos + 1));
    for (; i > 0;) {
        if (!bs[(u8) (_p[--i])]) return i;
    }

    return npos;
}

size_t fastring::find_last_not_of(char c, size_t pos) const {
    if (this->empty()) return npos;

    size_t i = (pos >= _size ? _size : (pos + 1));
    for (; i > 0;) {
        if (_p[--i] != c) return i;
    }

    return npos;
}

fastring& fastring::replace(const char* sub, const char* to, size_t maxreplace) {
    if (this->empty()) return *this;

    const char* from = this->c_str();
    const char* p = strstr(from, sub);
    if (!p) return *this;

    size_t n = strlen(sub);
    size_t m = strlen(to);

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

fastring& fastring::strip(const char* s, char d) {
    if (this->empty()) return *this;

    typedef unsigned char u8;
    char bs[256] = { 0 };
    while (*s) bs[(const u8)(*s++)] = 1;

    if (d == 'l' || d == 'L') {
        size_t b = 0;
        while (b < _size && bs[(u8)(_p[b])]) ++b;
        if (b != 0 && (_size -= b) != 0) memmove(_p, _p + b, _size);

    } else if (d == 'r' || d == 'R') {
        size_t e = _size;
        while (e > 0 && bs[(u8)(_p[e - 1])]) --e;
        if (e != _size) _size = e;

    } else {
        size_t e = _size;
        while (e > 0 && bs[(u8)(_p[e - 1])]) --e;
        if (e != _size) _size = e;
        if (e == 0) return *this;

        size_t b = 0;
        while (b < _size && bs[(u8)(_p[b])]) ++b;
        if (b != 0 && (_size -= b) != 0) memmove(_p, _p + b, _size);
    }

    return *this;
}

static bool _Match(const char* e, const char* p) {
    if (*p == '*' && !p[1]) return true;

    for (; *p && *e;) {
        if (*p == '*') return _Match(e, p + 1) || _Match(e + 1, p);
        if (*p != '?' && *p != *e) return false;
        ++p, ++e;
    }

    return (*p == '*' && !p[1]) || (!*p && !*e);
}

bool fastring::match(const char* p) const {
    return _Match(this->c_str(), p);
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
