#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4200)
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ostream>

// !! max length: 4G - 1
class fastring {
  public:
    static const size_t npos = (size_t)-1;

    constexpr fastring() noexcept : _p(0) {}

    explicit fastring(size_t cap) {
        this->_Init(cap);
    }

    fastring(const void* s, size_t n) {
        this->_Init(n + 1, n);
        memcpy(_p->s, s, n);
    }

    fastring(const char* s);

    fastring(size_t n, char c) {
        this->_Init(n + 1, n);
        memset(_p->s, c, n);
    }

    fastring(char c, size_t n) {
        this->_Init(n + 1, n);
        memset(_p->s, c, n);
    }

    static const size_t header_size() {
        return sizeof(_Mem);
    }

    fastring(char* p, size_t cap, size_t size) {
        _p = (_Mem*) p;
        _p->cap = cap - this->header_size();
        _p->size = size - this->header_size();
        _p->refn = 1;
    }

    ~fastring() {
        if (!_p) return;
        if (--_p->refn == 0) free(_p);
        _p = 0;
    }

    fastring(fastring&& s) noexcept : _p(s._p) {
        s._p = 0;
    }

    fastring(const fastring& s) _p(s._p) {
        if (_p) ++_p->refn;
    }

    fastring& operator=(fastring&& s) noexcept {
        if (_p && --_p->refn == 0) free(_p);
        _p = s._p;
        s._p = 0;
        return *this;
    }

    fastring& operator=(const fastring& s) {
        if (&s != this) {
            if (_p && --_p->refn == 0) free(_p);
            _p = s._p;
            if (_p) ++_p->refn;
        }
        return *this;
    }

    fastring& operator=(const char* s);

    const char* data() const {
        return _p ? _p->s : (const char*) &_p;
    }

    size_t size() const {
        return _p ? _p->size : 0;
    }

    bool empty() const {
        return !_p || _p->size == 0;
    }

    size_t capacity() const {
        return _p ? _p->cap : 0;
    }

    const char* c_str() const {
        if (!_p) return (const char*) &_p;
        ((fastring*)this)->_Reserve(_p->size + 1);
        if (_p->s[_p->size] != '\0') _p->s[_p->size] = '\0';
        return _p->s;
    }

    void clear() {
        if (_p) _p->size = 0;
    }

    // !! newly allocated memory is not initialized
    void resize(size_t n) {
        _p ? this->_Reserve(n) : this->_Init(n);
        _p->size = (unsigned) n;
    }

    void reserve(size_t n) {
        _p ? this->_Reserve(n) : this->_Init(n);
    }

    char& back() const {
        return _p->s[_p->size - 1];
    }

    char& front() const {
        return _p->s[0];
    }

    char& operator[](size_t i) const {
        return _p->s[i];
    }

    fastring& append(const void* p, size_t n) {
        return this->_Append_safe(p, n);
    }

    fastring& append(const char* s) {
        return this->_Append_safe(s, strlen(s));
    }

    fastring& append(char c) {
        _p ? this->_Ensure(1) : this->_Init(16);
        _p->s[_p->size++] = c;
        return *this;
    }

    fastring& append(char c, size_t n) {
        _p ? this->_Ensure(n) : this->_Init(n + 1);
        memset(_p->s + _p->size, c, n);
        _p->size += (unsigned) n;
        return *this;
    }

    fastring& append(size_t n, char c) {
        return this->append(c, n);
    }

    fastring& append(const fastring& s);

    template<typename S>
    fastring& append(const S& s) {
        return this->_Append(s.data(), s.size());
    }

    fastring& operator+=(const char* s) {
        return this->append(s);
    }

    fastring& operator+=(const fastring& s) {
        return this->append(s);
    }

    template<typename S>
    fastring& operator+=(const S& s) {
        return this->append(s);
    }

    fastring& operator+=(char c) {
        return this->append(c);
    }

    fastring clone() const {
        return _p ? fastring(_p->s, _p->size) : fastring();
    }

    fastring substr(size_t pos) const {
        if (pos >= this->size()) return fastring();
        return fastring(_p->s + pos, (size_t)_p->size - pos);
    }

    fastring substr(size_t pos, size_t len) const {
        if (pos >= this->size()) return fastring();
        size_t n = (size_t)_p->size - pos;
        return fastring(_p->s + pos, len < n ? len : n);
    }

    size_t find(char c) const {
        if (this->empty()) return npos;
        char* p = (char*) memchr(_p->s, c, _p->size);
        return p ? p - _p->s : npos;
    }

    size_t find(char c, size_t pos) const {
        if (pos >= this->size()) return npos;
        char* p = (char*) memchr(_p->s + pos, c, (size_t)_p->size - pos);
        return p ? p - _p->s : npos;
    }

    size_t find(const char* s) const;
    size_t find(const char* s, size_t pos) const;

    size_t rfind(char c) const;
    size_t rfind(const char* s) const;

    size_t find_first_of(const char* s) const;
    size_t find_first_not_of(const char* s) const;
    size_t find_first_not_of(char c) const;

    size_t find_last_of(const char* s) const;
    size_t find_last_not_of(const char* s) const;
    size_t find_last_not_of(char c) const;

    void replace(const char* sub, const char* to, unsigned maxreplace=-1);

    void strip(const char* s=" \t\r\n", char d='b');
    
    void strip(char c, char d='b') {
        char s[2] = { c, '\0' };
        this->strip((const char*)s, d);
    }

    bool starts_with(char c) const {
        return !this->empty() && this->front() == c;
    }

    bool starts_with(const char* s, size_t n) const {
        if (n == 0) return true;
        return this->size() >= n && memcmp(_p->s, s, n) == 0;
    }

    bool starts_with(const char* s) const {
        return this->starts_with(s, strlen(s));
    }

    template<typename S>
    bool starts_with(const S& s) const {
        return this->starts_with(s.data(), s.size());
    }

    bool ends_with(char c) const {
        return !this->empty() && this->back() == c;
    }

    bool ends_with(const char* s, size_t n) const {
        if (n == 0) return true;
        return this->size() >= n && memcmp(_p->s + this->size() - n, s, n) == 0;
    }

    bool ends_with(const char* s) const {
        return this->ends_with(s, strlen(s));
    }

    template<typename S>
    bool ends_with(const S& s) const {
        return this->ends_with(s.data(), s.size());
    }

    // * matches everything
    // ? matches any single character
    bool match(const char* pattern) const;

    fastring& toupper();
    fastring& tolower();

    fastring upper() const {
        return this->empty() ? fastring() : fastring(_p->s, _p->size).toupper();
    }

    fastring lower() const {
        return this->empty() ? fastring() : fastring(_p->s, _p->size).tolower();
    }

    void swap(fastring& s) noexcept {
        _Mem* p = _p;
        _p = s._p;
        s._p = p;
    }

    void swap(fastring&& s) noexcept {
        s.swap(*this);
    }

  private:
    void _Init(size_t cap, size_t size = 0);
    void _Reserve(size_t n);

    void _Ensure(size_t n) {
        if (_p->cap < _p->size + (unsigned)n) {
            this->_Reserve((_p->cap * 3 >> 1) + (unsigned)n);
        }
    }

    bool _Inside(const char* p) const {
        return _p->s <= p && p < _p->s + _p->size;
    }

    fastring& _Append(const void* p, size_t n) {
        _p ? this->_Ensure(n): this->_Init(n + 1);
        memcpy(_p->s + _p->size, p, n);
        _p->size += (unsigned) n;
        return *this;
    }

    fastring& _Append_safe(const void* p, size_t n);

  private:
    struct _Mem {
        unsigned cap;
        unsigned size;
        unsigned refn;
        unsigned ____;
        char s[];
    }; // 16 bytes

    _Mem* _p;
};

inline fastring operator+(const fastring& a, char b) {
    return fastring(a.size() + 2).append(a).append(b);
}

inline fastring operator+(char a, const fastring& b) {
    return fastring(b.size() + 2).append(a).append(b);
}

inline fastring operator+(const fastring& a, const fastring& b) {
    return fastring(a.size() + b.size() + 1).append(a).append(b);
}

inline fastring operator+(const fastring& a, const char* b) {
    size_t n = strlen(b);
    return fastring(a.size() + n + 1).append(a).append(b, n);
}

inline fastring operator+(const char* a, const fastring& b) {
    size_t n = strlen(a);
    return fastring(b.size() + n + 1).append(a, n).append(b);
}

inline bool operator==(const fastring& a, const fastring& b) {
    if (a.size() != b.size()) return false;
    return a.size() == 0 || memcmp(a.data(), b.data(), a.size()) == 0;
}

inline bool operator==(const fastring& a, const char* b) {
    if (a.size() != strlen(b)) return false;
    return a.size() == 0 || memcmp(a.data(), b, a.size()) == 0;
}

inline bool operator==(const char* a, const fastring& b) {
    return b == a;
}

inline bool operator!=(const fastring& a, const fastring& b) {
    return !(a == b);
}

inline bool operator!=(const fastring& a, const char* b) {
    return !(a == b);
}

inline bool operator!=(const char* a, const fastring& b) {
    return b != a;
}

inline bool operator<(const fastring& a, const fastring& b) {
    if (a.size() < b.size()) {
        return a.size() == 0 || memcmp(a.data(), b.data(), a.size()) <= 0;
    } else {
        return memcmp(a.data(), b.data(), b.size()) < 0;
    }
}

inline bool operator<(const fastring& a, const char* b) {
    size_t n = strlen(b);
    if (a.size() < n) {
        return a.size() == 0 || memcmp(a.data(), b, a.size()) <= 0;
    } else {
        return memcmp(a.data(), b, n) < 0;
    }
}

inline bool operator>(const fastring& a, const fastring& b) {
    if (a.size() > b.size()) {
        return b.size() == 0 || memcmp(a.data(), b.data(), b.size()) >= 0;
    } else {
        return memcmp(a.data(), b.data(), a.size()) > 0;
    }
}

inline bool operator>(const fastring& a, const char* b) {
    size_t n = strlen(b);
    if (a.size() > n) {
        return n == 0 || memcmp(a.data(), b, n) >= 0;
    } else {
        return memcmp(a.data(), b, a.size()) > 0;
    }
}

inline bool operator<(const char* a, const fastring& b) {
    return b > a;
}

inline bool operator>(const char* a, const fastring& b) {
    return b < a;
}

inline bool operator<=(const fastring& a, const fastring& b) {
    return !(a > b);
}

inline bool operator<=(const fastring& a, const char* b) {
    return !(a > b);
}

inline bool operator<=(const char* a, const fastring& b) {
    return !(b < a);
}

inline bool operator>=(const fastring& a, const fastring& b) {
    return !(a < b);
}

inline bool operator>=(const fastring& a, const char* b) {
    return !(a < b);
}

inline bool operator>=(const char* a, const fastring& b) {
    return !(b > a);
}

inline void swap(fastring& lhs, fastring& rhs) noexcept {
    lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& os, const fastring& s) {
    return os << s.c_str();
}
