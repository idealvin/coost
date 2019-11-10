#include "fastring.h"

void fastring::_Init(size_t cap, size_t size) {
    _p = (_Mem*) malloc(sizeof(_Mem) + cap);
    _p->cap = (unsigned) cap;
    _p->size = (unsigned) size;
    _p->refn = 1;
}

fastring::fastring(const char* s) {
    if (!*s) { _p = 0; return; }
    size_t n = strlen(s);
    this->_Init(n + 1, n);
    memcpy(_p->s, s, n + 1); // '\0' also copied
}

fastring& fastring::operator=(const char* s) {
    if (!*s) { this->clear(); return *this; }

    size_t n = strlen(s);
    if (_p) {
        if (!this->_Inside(s)) {
            this->_Reserve(n + 1);
            memcpy(_p->s, s, n + 1);
            _p->size = (unsigned) n;
        } else if (s != _p->s) {
            assert(n <= _p->size);
            memmove(_p->s, s, n + 1); // memory may overlap
            _p->size = (unsigned) n;
        }
    } else {
        this->_Init(n + 1, n);
        memcpy(_p->s, s, n + 1);
    }
    return *this;
}

fastring& fastring::append(const fastring& s) {
    if (&s != this) {
        return this->_Append(s.data(), s.size());
    } else if (_p) {
        this->_Reserve(_p->size << 1);
        memcpy(_p->s + _p->size, _p->s, _p->size);
        _p->size <<= 1;
    }
    return *this;
}

size_t fastring::find(const char* s) const {
    if (this->empty()) return npos;
  #ifdef __linux__
    char* p = (char*) memmem(_p->s, _p->size, s, strlen(s));
  #else
    const char* p = strstr(this->c_str(), s);
  #endif
    return p ? p - _p->s : npos;
}

size_t fastring::find(const char* s, size_t pos) const {
    if (pos >= this->size()) return npos;
  #ifdef __linux__
    const char* p = (const char*) memmem(_p->s + pos, (size_t)_p->size - pos, s, strlen(s));
  #else
    const char* p = strstr(this->c_str() + pos, s);
  #endif
    return p ? p - _p->s : npos;
}

size_t fastring::rfind(char c) const {
    if (this->empty()) return npos;
  #ifdef __linux__
    char* p = (char*) memrchr(_p->s, c, _p->size);
  #else
    const char* p = (const char*) strrchr(this->c_str(), c);
  #endif
    return p ? p - _p->s : npos;
}

size_t fastring::rfind(const char* sub) const {
    unsigned int m = (unsigned int) strlen(sub);
    if (m == 1) return this->rfind(*sub);

    unsigned int n = (unsigned int) this->size();
    if (n < m) return npos;

    const unsigned char* s = (const unsigned char*) _p->s;
    const unsigned char* p = (const unsigned char*) sub;

    unsigned int tbl[256] = { 0 };
    for (unsigned int i = m; i > 0; --i) tbl[p[i - 1]] = i;

    for (unsigned int j = n - m;;) {
        if (memcmp(p, s + j, m) == 0) return j;
        if (j == 0) return npos;

        unsigned int x = tbl[s[j - 1]];
        if (x == 0) x = m + 1;
        if (j < x) return npos;
        j -= x;
    }

    return npos;
}

size_t fastring::find_first_of(const char* s) const {
    if (this->empty()) return npos;
    size_t r = strcspn(this->c_str(), s);
    return _p->s[r] ? r : npos;
}

size_t fastring::find_first_not_of(const char* s) const {
    if (this->empty()) return npos;
    size_t r = strspn(this->c_str(), s);
    return _p->s[r] ? r : npos;
}

size_t fastring::find_first_not_of(char c) const {
    char buf[2] = { c, '\0' };
    return this->find_first_not_of((const char*)buf);
}

size_t fastring::find_last_of(const char* s) const {
    if (this->empty()) return npos;

    typedef unsigned char u8;
    char bs[256] = { 0 };
    while (*s) bs[(const u8) (*s++)] = 1;

    for (unsigned i = _p->size; i > 0;) {
        if (bs[(u8) (_p->s[--i])]) return i;
    }

    return npos;
}

size_t fastring::find_last_not_of(const char* s) const {
    if (this->empty()) return npos;

    typedef unsigned char u8;
    char bs[256] = { 0 };
    while (*s) bs[(const u8) (*s++)] = 1;

    for (unsigned i = _p->size; i > 0;) {
        if (!bs[(u8) (_p->s[--i])]) return i;
    }

    return npos;
}

size_t fastring::find_last_not_of(char c) const {
    if (!_p) return npos;
    for (unsigned i = _p->size; i > 0;) {
        if (_p->s[--i] != c) return i;
    }
    return npos;
}

void fastring::replace(const char* sub, const char* to, unsigned maxreplace) {
    if (!_p) return;

    const char* from = this->c_str();
    const char* p = strstr(from, sub);
    if (!p) return;

    size_t n = strlen(sub);
    size_t m = strlen(to);

    fastring s(_p->size);

    do {
        s.append(from, p - from).append(to, m);
        from = p + n;
        if (--maxreplace == 0) break;
    } while ((p = strstr(from, sub)));

    if (from < _p->s + _p->size) s.append(from);
    this->resize(s.size());
    memcpy(_p->s, s.data(), s.size());
}

void fastring::strip(const char* s, char d) {
    if (this->empty()) return;

    typedef unsigned char u8;
    char bs[256] = { 0 };
    while (*s) bs[(const u8) (*s++)] = 1;

    if (d == 'l' || d == 'L') {
        unsigned b = 0;
        while (b < _p->size && bs[(u8)(_p->s[b])]) ++b;
        if (b == 0 || (_p->size -= b) == 0) return;
        memmove(_p->s, _p->s + b, _p->size);

    } else if (d == 'r' || d == 'R') {
        unsigned e = _p->size;
        while (e > 0 && bs[(u8)(_p->s[e - 1])]) --e;
        if (e != _p->size) _p->size = e;

    } else {
        unsigned e = _p->size;
        while (e > 0 && bs[(u8)(_p->s[e - 1])]) --e;
        if (e != _p->size) _p->size = e;
        if (e == 0) return;

        unsigned b = 0;
        while (b < _p->size && bs[(u8)(_p->s[b])]) ++b;
        if (b == 0 || (_p->size -= b) == 0) return;
        memmove(_p->s, _p->s + b, _p->size);
    }
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
    if (!_p) return *this;
    for (unsigned i = 0; i < _p->size; ++i) {
        char& c = _p->s[i];
        if ('a' <= c && c <= 'z') c ^= 32;
    }
    return *this;
}

fastring& fastring::tolower() {
    if (!_p) return *this;
    for (unsigned i = 0; i < _p->size; ++i) {
        char& c = _p->s[i];
        if ('A' <= c && c <= 'Z') c ^= 32;
    }
    return *this;
}

void fastring::_Reserve(size_t n) {
    if (_p->cap < n) {
        if (_p->refn == 1) {
            _p = (_Mem*) realloc(_p, n + sizeof(_Mem));
            assert(_p);
            _p->cap = (unsigned) n;
        } else {
            --_p->refn;
            char* s = _p->s;
            this->_Init(n, _p->size);
            memcpy(_p->s, s, _p->size);
        }
    }
}

fastring& fastring::_Append_safe(const void* x, size_t n) {
    const char* p = (const char*) x;
    if (_p) {
        if (!this->_Inside(p)) {
            this->_Ensure(n);
            memcpy(_p->s + _p->size, p, n);
        } else {
            assert(p + n <= _p->s + _p->size);
            size_t pos = p - _p->s;
            this->_Ensure(n);
            memcpy(_p->s + _p->size, _p->s + pos, n);
        }
        _p->size += (unsigned) n;
    } else {
        this->_Init(n + 1, n);
        memcpy(_p->s, p, n);
    }
    return *this;        
}
