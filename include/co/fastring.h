#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4706) // if ((a = x))
#endif

#include "fast.h"
#include "hash/murmur_hash.h"
#include <string>
#include <ostream>

class __coapi fastring : public fast::stream {
  public:
    static const size_t npos = (size_t)-1;

    constexpr fastring() noexcept
        : fast::stream() {
    }

    explicit fastring(size_t cap)
        : fast::stream(cap) {
    }

    ~fastring() = default;

    fastring(const void* s, size_t n)
        : fast::stream(n, n) {
        memcpy(_p, s, n);
    }

    fastring(size_t n, char c)
        : fast::stream(n, n) {
        memset(_p, c, n);
    }

    fastring(char c, size_t n) : fastring(n, c) {}

    fastring(const char* s)
        : fastring(s, strlen(s)) {
    }

    fastring(const std::string& s)
        : fastring(s.data(), s.size()) {
    }

    fastring(const fastring& s)
        : fastring(s.data(), s.size()) {
    }

    fastring(fastring&& s) noexcept
        : fast::stream(std::move(s)) {
    }

    fastring& operator=(fastring&& s) {
        return (fastring&) fast::stream::operator=(std::move(s));
    }

    fastring& operator=(const fastring& s) {
        return &s != this ? this->_assign(s.data(), s.size()) : *this;
    }

    fastring& operator=(const std::string& s) {
        return this->_assign(s.data(), s.size());
    }

    fastring& operator=(const char* s) {
        return this->assign(s, strlen(s));
    }

    // It is ok if s overlaps with the internal buffer of fastring
    fastring& assign(const void* s, size_t n) {
        if (!this->_inside((const char*)s)) return this->_assign(s, n);
        assert((const char*)s + n <= _p + _size);
        if (s != _p) memmove(_p, s, n);
        _size = n;
        return *this;
    }

    template<typename S>
    fastring& assign(S&& s) {
        return this->operator=(std::forward<S>(s));
    }

    // It is ok if s overlaps with the internal buffer of fastring
    fastring& append(const void* s, size_t n) {
        return (fastring&) fast::stream::safe_append(s, n);
    }
 
    // It is ok if s overlaps with the internal buffer of fastring
    fastring& append(const char* s) {
        return this->append(s, strlen(s));
    }

    // It is ok if s is the fastring itself
    fastring& append(const fastring& s) {
        if (&s != this) return (fastring&) fast::stream::append(s.data(), s.size());
        this->reserve(_size << 1);
        memcpy(_p + _size, _p, _size); // append itself
        _size <<= 1;
        return *this;
    }

    fastring& append(const std::string& s) {
        return (fastring&) fast::stream::append(s.data(), s.size());
    }

    fastring& append(size_t n, char c) {
        return (fastring&) fast::stream::append(n, c);
    }

    fastring& append(char c, size_t n) {
        return this->append(n, c);
    }

    fastring& append(char c) {
        return (fastring&)fast::stream::append(c);
    }

    fastring& append(signed char c) {
        return this->append((char)c);
    }

    fastring& append(unsigned char c) {
        return this->append((char)c);
    }

    template<typename T>
    fastring& operator+=(T&& t) {
        return this->append(std::forward<T>(t));
    }

    fastring& cat() { return *this; }

    // concatenate fastring to any number of elements
    //   - fastring s("hello");
    //     s.cat(' ', 123);  // s -> "hello 123"
    template<typename X, typename ...V>
    fastring& cat(X&& x, V&& ... v) {
        (*this) << std::forward<X>(x);
        return this->cat(std::forward<V>(v)...);
    }

    fastring& operator<<(bool v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(char v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(signed char v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(unsigned char v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(short v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(unsigned short v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(int v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(unsigned int v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(long v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(unsigned long v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(long long v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(unsigned long long v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(double v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(float v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    // float point number with max decimal places set
    //   - fastring() << dp::_2(3.1415);  // -> 3.14
    fastring& operator<<(const dp::_fpt& v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(const void* v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(std::nullptr_t) {
        return (fastring&) fast::stream::operator<<(nullptr);
    }

    fastring& operator<<(const char* s) {
        return this->append(s, strlen(s));
    }

    fastring& operator<<(const signed char* s) {
        return this->operator<<((const char*)s);
    }

    fastring& operator<<(const unsigned char* s) {
        return this->operator<<((const char*)s);
    }

    fastring& operator<<(const fastring& s) {
        return this->append(s);
    }

    fastring& operator<<(const std::string& s) {
        return this->append(s);
    }

    bool contains(char c) const {
        return this->find(c) != npos;
    }

    bool contains(const char* s) const {
        return this->find(s) != npos;
    }

    bool contains(const fastring& s) const {
        return this->contains(s.c_str());
    }

    bool contains(const std::string& s) const {
        return this->contains(s.c_str());
    }

    bool starts_with(char c) const {
        return !this->empty() && this->front() == c;
    }

    bool starts_with(const char* s, size_t n) const {
        return n == 0 || (n <= _size && memcmp(_p, s, n) == 0);
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
        return n == 0 || (n <= _size && memcmp(_p + _size - n, s, n) == 0);
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

    fastring& remove_prefix(const char* s, size_t n) {
        return this->starts_with(s, n) ? this->trim(n, 'l') : *this;
    }

    fastring& remove_prefix(const char* s) {
        return this->remove_prefix(s, strlen(s));
    }

    fastring& remove_prefix(const fastring& s) {
        return this->remove_prefix(s.data(), s.size());
    }

    fastring& remove_prefix(const std::string& s) {
        return this->remove_prefix(s.data(), s.size());
    }

    fastring& remove_suffix(const char* s, size_t n) {
        if (this->ends_with(s, n)) this->resize(this->size() - n); 
        return *this;
    }

    fastring& remove_suffix(const char* s) {
        return this->remove_suffix(s, strlen(s));
    }

    fastring& remove_suffix(const fastring& s) {
        return this->remove_suffix(s.data(), s.size());
    }

    fastring& remove_suffix(const std::string& s) {
        return this->remove_suffix(s.data(), s.size());
    }

    // remove character @c on the left or right side, or both sides
    // @d: 'l' or 'L' for left, 'r' or 'R' for right, otherwise for both sides
    fastring& trim(char c, char d='b');

    fastring& trim(unsigned char c, char d='b') {
        return this->trim((char)c, d);
    }

    fastring& trim(signed char c, char d='b') {
        return this->trim((char)c, d);
    }

    // remove characters in @s on the left or right side, or both sides
    // @d: 'l' or 'L' for left, 'r' or 'R' for right, otherwise for both sides
    fastring& trim(const char* s=" \t\r\n", char d='b');
    
    // remove the first n characters or the last n characters, or both
    // @d: 'l' or 'L' for left, 'r' or 'R' for right, otherwise for both sides
    fastring& trim(size_t n, char d='b');

    fastring& trim(int n, char d='b') {
        return this->trim((size_t)n, d);
    }

    // the same as trim
    template<typename ...X>
    fastring& strip(X&& ...x) {
        return this->trim(std::forward<X>(x)...);
    }

    // replace @sub in the string with @to, do not apply it to binary strings
    // @maxreplace: 0 for unlimited
    fastring& replace(const char* sub, const char* to, size_t maxreplace=0);

    fastring& tolower();
    fastring& toupper();

    fastring lower() const {
        fastring s(*this);
        s.tolower();
        return s;
    }

    fastring upper() const {
        fastring s(*this);
        s.toupper();
        return s;
    }

    fastring substr(size_t pos) const {
        return pos < _size ? fastring(_p + pos, _size - pos) : fastring();
    }

    fastring substr(size_t pos, size_t len) const {
        if (pos < _size) {
            const size_t n = _size - pos;
            return fastring(_p + pos, len < n ? len : n);
        }
        return fastring();
    }

    // find character @c
    size_t find(char c) const {
        if (!this->empty()) {
            char* const p = (char*) memchr(_p, c, _size);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find character @c from @pos
    size_t find(char c, size_t pos) const {
        if (pos < _size) {
            char* const p = (char*) memchr(_p + pos, c, _size - pos);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find character @c in [pos, pos + len)
    size_t find(char c, size_t pos, size_t len) const {
        if (pos < _size) {
            const size_t n = _size - pos;
            char* const p = (char*) memchr(_p + pos, c, len < n ? len : n);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find sub string with strstr, do not apply it to binary strings
    size_t find(const char* s) const {
        if (!this->empty()) {
            const char* const p = strstr(this->c_str(), s);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find sub string from @pos, do not apply it to binary strings
    size_t find(const char* s, size_t pos) const {
        if (pos < _size) {
            const char* const p = strstr(this->c_str() + pos, s);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // reverse find character @c, do not apply it to binary strings
    size_t rfind(char c) const {
        if (!this->empty()) {
            const char* const p = strrchr(this->c_str(), c);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // reverse find sub string, do not apply it to binary strings
    size_t rfind(const char* s) const;

    // find first char in @s, do not apply it to binary strings
    size_t find_first_of(const char* s) const {
        if (!this->empty()) {
            const size_t r = strcspn(this->c_str(), s);
            return _p[r] ? r : npos;
        }
        return npos;
    }

    // find first char in @s from @pos, do not apply it to binary strings
    size_t find_first_of(const char* s, size_t pos) const {
        if (pos < _size) {
            const size_t r = strcspn(this->c_str() + pos, s) + pos;
            return _p[r] ? r : npos;
        }
        return npos;
    }

    // find first char not in @s, do not apply it to binary strings
    size_t find_first_not_of(const char* s) const {
        if (!this->empty()) {
            const size_t r = strspn(this->c_str(), s);
            return _p[r] ? r : npos;
        }
        return npos;
    }

    // find first char not in @s from @pos, do not apply it to binary strings
    size_t find_first_not_of(const char* s, size_t pos) const {
        if (pos < _size) {
            const size_t r = strspn(this->c_str() + pos, s) + pos;
            return _p[r] ? r : npos;
        }
        return npos;
    }

    // find first char not equal to @c
    size_t find_first_not_of(char c, size_t pos=0) const;

    // find last char in @s
    size_t find_last_of(const char* s, size_t pos=npos) const;

    // find last char not in @s
    size_t find_last_not_of(const char* s, size_t pos=npos) const;

    // find last char not equal to @c
    size_t find_last_not_of(char c, size_t pos=npos) const;

    // * matches everything
    // ? matches any single character
    bool match(const char* pattern) const;

    void shrink() {
        if (_size + 1 < _cap) this->swap(fastring(*this));
    }

  private:
    fastring& _assign(const void* s, size_t n) {
        _size = n;
        if (n > 0) {
            this->reserve(n);
            memcpy(_p, s, n);
        }
        return *this;
    }

    bool _inside(const char* p) const {
        return _p <= p && p < _p + _size;
    }
};

inline fastring operator+(const fastring& a, char b) {
    fastring s(a.size() + 2);
    s.append(a).append(b);
    return s;
}

inline fastring operator+(char a, const fastring& b) {
    fastring s(b.size() + 2);
    s.append(a).append(b);
    return s;
}

inline fastring operator+(const fastring& a, const fastring& b) {
    fastring s(a.size() + b.size() + 1);
    s.append(a).append(b);
    return s;
}

inline fastring operator+(const fastring& a, const std::string& b) {
    fastring s(a.size() + b.size() + 1);
    s.append(a).append(b);
    return s;
}

inline fastring operator+(const std::string& a, const fastring& b) {
    fastring s(a.size() + b.size() + 1);
    s.append(a).append(b);
    return s;
}

inline fastring operator+(const fastring& a, const char* b) {
    const size_t n = strlen(b);
    fastring s(a.size() + n + 1);
    s.append(a).append(b, n);
    return s;
}

inline fastring operator+(const char* a, const fastring& b) {
    const size_t n = strlen(a);
    fastring s(b.size() + n + 1);
    s.append(a, n).append(b);
    return s;
}

inline bool operator==(const fastring& a, const fastring& b) {
    return a.size() == b.size() && (a.empty() || memcmp(a.data(), b.data(), a.size()) == 0);
}

inline bool operator==(const fastring& a, const std::string& b) {
    return a.size() == b.size() && (a.empty() || memcmp(a.data(), b.data(), a.size()) == 0);
}

inline bool operator==(const std::string& a, const fastring& b) {
    return b == a;
}

inline bool operator==(const fastring& a, const char* b) {
    return a.size() == strlen(b) && (a.empty() || memcmp(a.data(), b, a.size()) == 0);
}

inline bool operator==(const char* a, const fastring& b) {
    return b == a;
}

inline bool operator!=(const fastring& a, const fastring& b) {
    return !(a == b);
}

inline bool operator!=(const fastring& a, const std::string& b) {
    return !(a == b);
}

inline bool operator!=(const std::string& a, const fastring& b) {
    return !(a == b);
}

inline bool operator!=(const fastring& a, const char* b) {
    return !(a == b);
}

inline bool operator!=(const char* a, const fastring& b) {
    return b != a;
}

inline bool operator<(const fastring& a, const fastring& b) {
    return a.size() < b.size()
        ? (a.empty() || memcmp(a.data(), b.data(), a.size()) <= 0)
        : (memcmp(a.data(), b.data(), b.size()) < 0);
}

inline bool operator<(const fastring& a, const std::string& b) {
    return a.size() < b.size()
        ? (a.empty() || memcmp(a.data(), b.data(), a.size()) <= 0)
        : (memcmp(a.data(), b.data(), b.size()) < 0);
}

inline bool operator<(const std::string& a, const fastring& b) {
    return a.size() < b.size()
        ? (a.empty() || memcmp(a.data(), b.data(), a.size()) <= 0)
        : (memcmp(a.data(), b.data(), b.size()) < 0);
}

inline bool operator<(const fastring& a, const char* b) {
    const size_t n = strlen(b);
    return a.size() < n
        ? (a.empty() || memcmp(a.data(), b, a.size()) <= 0)
        : (memcmp(a.data(), b, n) < 0);
}

inline bool operator>(const fastring& a, const fastring& b) {
    return a.size() > b.size()
        ? (b.empty() || memcmp(a.data(), b.data(), b.size()) >= 0)
        : (memcmp(a.data(), b.data(), a.size()) > 0);
}

inline bool operator>(const fastring& a, const std::string& b) {
    return a.size() > b.size()
        ? (b.empty() || memcmp(a.data(), b.data(), b.size()) >= 0)
        : (memcmp(a.data(), b.data(), a.size()) > 0);
}

inline bool operator>(const std::string& a, const fastring& b) {
    return a.size() > b.size()
        ? (b.empty() || memcmp(a.data(), b.data(), b.size()) >= 0)
        : (memcmp(a.data(), b.data(), a.size()) > 0);
}

inline bool operator>(const fastring& a, const char* b) {
    const size_t n = strlen(b);
    return a.size() > n
        ? (n == 0 || memcmp(a.data(), b, n) >= 0)
        : (memcmp(a.data(), b, a.size()) > 0);
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

inline bool operator<=(const fastring& a, const std::string& b) {
    return !(a > b);
}

inline bool operator<=(const std::string& a, const fastring& b) {
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

inline bool operator>=(const fastring& a, const std::string& b) {
    return !(a < b);
}

inline bool operator>=(const std::string& a, const fastring& b) {
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

namespace std {
template<>
struct hash<fastring> {
    size_t operator()(const fastring& s) const {
        return murmur_hash(s.data(), s.size());
    }
};
} // std

class anystr {
  public:
    constexpr anystr() noexcept : _s(""), _n(0) {}
    constexpr anystr(const char* s, size_t n) noexcept : _s(s), _n(n) {}

    // modern compilers may do strlen for string literals at compile time,
    // see https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
    anystr(const char* s) noexcept : _s(s), _n(strlen(s)) {}

    anystr(const std::string& s) noexcept : _s(s.data()), _n(s.size()) {}
    anystr(const fastring& s) noexcept : _s(s.data()), _n(s.size()) {}

    constexpr const char* data() const noexcept { return _s; }
    constexpr size_t size() const noexcept { return _n; }

  private:
    const char* const _s;
    const size_t _n;
};
