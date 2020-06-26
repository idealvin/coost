#pragma once

#include "fast.h"
#include <ostream>

class fastring : public fast::bastream {
  public:
    static const size_t npos = (size_t)-1;

    constexpr fastring() noexcept : fast::bastream() {}
    
    explicit fastring(size_t cap) : fast::bastream(cap) {}

    ~fastring() = default;

    fastring(const void* s, size_t n) : fast::bastream(n + 1, n) {
        memcpy(_p, s, n);
    }

    fastring(const char* s) : fastring(s, strlen(s)) {}

    fastring(const std::string& s) : fastring(s.data(), s.size()) {}

    fastring(const fastring& s) : fastring(s.data(), s.size()) {}

    fastring(size_t n, char c) : fast::bastream(n + 1, n) {
        memset(_p, c, n);
    }

    fastring(char c, size_t n) : fastring(n, c) {}

    fastring(fastring&& s) noexcept
        : fast::bastream(std::move(s)) {
    }

    fastring& operator=(fastring&& s) noexcept {
        return (fastring&) fast::bastream::operator=(std::move(s));
    }

    fastring& operator=(const fastring& s);
    fastring& operator=(const std::string& s);
    fastring& operator=(const char* s);

    fastring& append(const void* p, size_t n);
 
    fastring& append(const char* s) {
        return this->append(s, strlen(s));
    }

    fastring& append(const fastring& s);

    fastring& append(const std::string& s) {
        return (fastring&) fast::bastream::append(s);
    }

    fastring& append(char c, size_t n) {
        return (fastring&) fast::bastream::append(c, n);
    }

    fastring& append(size_t n, char c) {
        return this->append(c, n);
    }

    template<typename T>
    fastring& append(T v) {
        return (fastring&) fast::bastream::append(v);
    }

    fastring& operator+=(const char* s) {
        return this->append(s);
    }

    fastring& operator+=(const fastring& s) {
        return this->append(s);
    }

    fastring& operator+=(const std::string& s) {
        return this->append(s);
    }

    fastring& operator+=(char c) {
        return this->append(c);
    }

    fastring& operator<<(const char* s) {
        return (fastring&) fast::bastream::operator<<(s);
    }

    fastring& operator<<(const std::string& s) {
        return (fastring&) fast::bastream::operator<<(s);
    }

    fastring& operator<<(const fastring& s) {
        return (fastring&) this->_Append(s.data(), s.size());
    }

    template<typename T>
    fastring& operator<<(T v) {
        return (fastring&) fast::bastream::operator<<(v);
    }

    fastring substr(size_t pos) const {
        if (this->size() <= pos) return fastring();
        return fastring(_p + pos, _size - pos);
    }

    fastring substr(size_t pos, size_t len) const {
        if (this->size() <= pos) return fastring();
        const size_t n = _size - pos;
        return fastring(_p + pos, len < n ? len : n);
    }

    // find, rfind, find_xxx_of are based on strrchr, strstr, strcspn, strspn, 
    // etc, and they are not applicable to strings containing binary characters.
    size_t find(char c) const {
        if (this->empty()) return npos;
        char* p = (char*) memchr(_p, c, _size);
        return p ? p - _p : npos;
    }

    size_t find(char c, size_t pos) const {
        if (this->size() <= pos) return npos;
        char* p = (char*) memchr(_p + pos, c, _size - pos);
        return p ? p - _p : npos;
    }

    size_t find(const char* s) const {
        if (this->empty()) return npos;
        const char* p = strstr(this->c_str(), s);
        return p ? p - _p : npos;
    }

    size_t find(const char* s, size_t pos) const {
        if (this->size() <= pos) return npos;
        const char* p = strstr(this->c_str() + pos, s);
        return p ? p - _p : npos;
    }

    size_t rfind(char c) const {
        if (this->empty()) return npos;
        const char* p = strrchr(this->c_str(), c);
        return p ? p - _p : npos;
    }

    size_t rfind(const char* s) const;

    size_t find_first_of(const char* s) const {
        if (this->empty()) return npos;
        size_t r = strcspn(this->c_str(), s);
        return _p[r] ? r : npos;
    }

    size_t find_first_not_of(const char* s) const {
        if (this->empty()) return npos;
        size_t r = strspn(this->c_str(), s);
        return _p[r] ? r : npos;
    }

    size_t find_first_not_of(char c) const {
        char s[2] = { c, '\0' };
        return this->find_first_not_of((const char*)s);
    }

    size_t find_last_of(const char* s) const;
    size_t find_last_not_of(const char* s) const;
    size_t find_last_not_of(char c) const;

    // @maxreplace: 0 for unlimited
    void replace(const char* sub, const char* to, size_t maxreplace=0);

    // @d: 'l' or 'L' for left, 'r' or 'R' for right
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
        return n <= this->size() && memcmp(_p, s, n) == 0;
    }

    bool starts_with(const char* s) const {
        return this->starts_with(s, strlen(s));
    }

    bool starts_with(const fastring& s) const {
        return this->starts_with(s.data(), s.size());
    }

    bool starts_with(const std::string& s) const {
        return this->starts_with(s.data(), s.size());
    }

    bool ends_with(char c) const {
        return !this->empty() && this->back() == c;
    }

    bool ends_with(const char* s, size_t n) const {
        if (n == 0) return true;
        return n <= this->size() && memcmp(_p + _size - n, s, n) == 0;
    }

    bool ends_with(const char* s) const {
        return this->ends_with(s, strlen(s));
    }

    bool ends_with(const fastring& s) const {
        return this->ends_with(s.data(), s.size());
    }

    bool ends_with(const std::string& s) const {
        return this->ends_with(s.data(), s.size());
    }

    // * matches everything
    // ? matches any single character
    bool match(const char* pattern) const;

    fastring& toupper();
    fastring& tolower();

    fastring upper() const {
        return fastring(*this).toupper();
    }

    fastring lower() const {
        return fastring(*this).tolower();
    }

  private:
    void _Init(size_t cap, size_t size, void* p) {
        _cap = cap;
        _size = size;
        _p = (char*) p;
    }

    bool _Inside(const char* p) const {
        return _p <= p && p < _p + _size;
    }
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

inline std::ostream& operator<<(std::ostream& os, const fastring& s) {
    return os.write(s.data(), s.size());
}
