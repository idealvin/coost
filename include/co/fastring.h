#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4706)
#endif

#include "god.h"
#include "fast.h"
#include "stref.h"
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

    fastring(void* p, size_t size, size_t cap) noexcept
        : fast::stream(p, size, cap) {
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

    template<typename S, god::enable_if_t<god::is_literal_string<god::remove_ref_t<S>>(), int> = 0>
    fastring(S&& s) {
        sizeof(s) > 1 ? _construct_from_c_str(s, sizeof(s)) : (void) new(this) fast::stream();
    }

    template<typename S, god::enable_if_t<god::is_c_str<god::remove_ref_t<S>>(), int> = 0>
    fastring(S&& s) {
        const size_t n = strlen(s) + 1;
        n > 1 ? _construct_from_c_str(s, n) : (void) new(this) fast::stream();
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
        if (&s != this) this->_assign(s.data(), s.size());
        return *this;
    }

    fastring& operator=(const std::string& s) {
        return this->_assign(s.data(), s.size());
    }

    template<typename S, god::enable_if_t<god::is_literal_string<god::remove_ref_t<S>>(), int> = 0>
    fastring& operator=(S&& s) {
        return this->_assign(s, sizeof(s) - 1);
    }

    template<typename S, god::enable_if_t<god::is_c_str<god::remove_ref_t<S>>(), int> = 0>
    fastring& operator=(S&& s) {
        return this->assign(s, strlen(s));
    }

    // s may points to part of the fastring itself
    fastring& assign(const void* s, size_t n) {
        if (!this->_is_inside((const char*)s)) return this->_assign(s, n);
        assert((const char*)s + n <= _p + _size);
        if (s != _p) memmove(_p, s, n);
        _size = n;
        return *this;
    }

    template<typename S>
    fastring& assign(S&& s) {
        return this->operator=(std::forward<S>(s));
    }

    fastring& append(const void* s, size_t n) {
        return (fastring&) fast::stream::safe_append(s, n);
    }
 
    fastring& append(const char* s) {
        return this->append(s, strlen(s));
    }

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

    fastring& append(const co::stref& s) {
        return (fastring&) fast::stream::safe_append(s.data(), s.size());
    }

    fastring& append(size_t n, char c) {
        return (fastring&) fast::stream::append(n, c);
    }

    fastring& append(char c, size_t n) {
        return this->append(n, c);
    }

    // append a single character
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
        this->operator<<(std::forward<X>(x));
        return this->cat(std::forward<V>(v)...);
    }

    friend class fpstream;
    class fpstream {
      public:
        fpstream(fastring* s, int mdp) noexcept : s(s), mdp(mdp) {}

        fpstream& operator<<(double v) {
            s->ensure(mdp + 8);
            s->_size += fast::dtoa(v, s->_p + s->_size, mdp);
            return *this;
        }

        fpstream& operator<<(float v) {
            return this->operator<<((double)v);
        }

        fpstream& operator<<(maxdp_t x) noexcept {
            mdp = x.n;
            return *this;
        }

        template <typename T>
        fpstream& operator<<(T&& t) {
            s->operator<<(std::forward<T>(t));
            return *this;
        }

        fastring* s;
        int mdp;
    };

    // set max decimal places for float point number, mdp must > 0
    fpstream maxdp(int mdp) noexcept {
        return fpstream(this, mdp);
    }

    // set max decimal places as mdp.n
    fpstream operator<<(maxdp_t mdp) {
        return fpstream(this, mdp.n);
    }

    fastring& operator<<(const signed char* s) {
        return this->operator<<((const char*)s);
    }

    fastring& operator<<(const unsigned char* s) {
        return this->operator<<((const char*)s);
    }

    template<typename S, god::enable_if_t<god::is_literal_string<god::remove_ref_t<S>>(), int> = 0>
    fastring& operator<<(S&& s) {
        return (fastring&) fast::stream::append(s, sizeof(s) - 1);
    }

    template<typename S, god::enable_if_t<god::is_c_str<god::remove_ref_t<S>>(), int> = 0>
    fastring& operator<<(S&& s) {
        return this->append(s, strlen(s));
    }

    fastring& operator<<(const fastring& s) {
        return this->append(s);
    }

    fastring& operator<<(const std::string& s) {
        return this->append(s);
    }

    fastring& operator<<(const co::stref& s) {
        return this->append(s.data(), s.size());
    }

    template<typename T, god::enable_if_t<god::is_basic<god::remove_ref_t<T>>(), int> = 0>
    fastring& operator<<(T&& t) {
        return (fastring&) fast::stream::operator<<(std::forward<T>(t));
    }

    template<typename T, god::enable_if_t<
        god::is_pointer<god::remove_ref_t<T>>() && !god::is_c_str<god::remove_ref_t<T>>(), int> = 0
    >
    fastring& operator<<(T&& p) {
        return (fastring&) fast::stream::operator<<(std::forward<T>(p));
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

    // find, rfind, find_xxx_of are implemented based on strrchr, strstr, 
    // strcspn, strspn, etc. Do not apply them to binary strings.
    size_t find(char c) const {
        if (!this->empty()) {
            char* const p = (char*) memchr(_p, c, _size);
            return p ? p - _p : npos;
        }
        return npos;
    }

    size_t find(char c, size_t pos) const {
        if (pos < _size) {
            char* const p = (char*) memchr(_p + pos, c, _size - pos);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find character c in [pos, pos + len)
    size_t find(char c, size_t pos, size_t len) const {
        if (pos < _size) {
            const size_t n = _size - pos;
            char* const p = (char*) memchr(_p + pos, c, len < n ? len : n);
            return p ? p - _p : npos;
        }
        return npos;
    }

    size_t find(const char* s) const {
        if (!this->empty()) {
            const char* p = strstr(this->c_str(), s);
            return p ? p - _p : npos;
        }
        return npos;
    }

    size_t find(const char* s, size_t pos) const {
        if (pos < _size) {
            const char* const p = strstr(this->c_str() + pos, s);
            return p ? p - _p : npos;
        }
        return npos;
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

    size_t find_first_of(const char* s, size_t pos) const {
        if (this->size() <= pos) return npos;
        size_t r = strcspn(this->c_str() + pos, s) + pos;
        return _p[r] ? r : npos;
    }

    size_t find_first_not_of(const char* s) const {
        if (this->empty()) return npos;
        size_t r = strspn(this->c_str(), s);
        return _p[r] ? r : npos;
    }

    size_t find_first_not_of(const char* s, size_t pos) const {
        if (this->size() <= pos) return npos;
        size_t r = strspn(this->c_str() + pos, s) + pos;
        return _p[r] ? r : npos;
    }

    size_t find_first_not_of(char c, size_t pos=0) const {
        char s[2] = { c, '\0' };
        return this->find_first_not_of((const char*)s, pos);
    }

    size_t find_last_of(const char* s, size_t pos=npos) const;
    size_t find_last_not_of(const char* s, size_t pos=npos) const;
    size_t find_last_not_of(char c, size_t pos=npos) const;

    // @maxreplace: 0 for unlimited
    fastring& replace(const char* sub, const char* to, size_t maxreplace=0);

    // @d: 'l' or 'L' for left, 'r' or 'R' for right
    fastring& strip(const char* s=" \t\r\n", char d='b');
    
    fastring& strip(char c, char d='b') {
        char s[2] = { c, '\0' };
        return this->strip((const char*)s, d);
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

    fastring& remove_tail(const char* s, size_t n) {
        if (this->ends_with(s, n)) this->resize(this->size() - n); 
        return *this;
    }

    fastring& remove_tail(const char* s) {
        return this->remove_tail(s, strlen(s));
    }

    fastring& remove_tail(const fastring& s) {
        return this->remove_tail(s.data(), s.size());
    }

    fastring& remove_tail(const std::string& s) {
        return this->remove_tail(s.data(), s.size());
    }

    // * matches everything
    // ? matches any single character
    bool match(const char* pattern) const;

    fastring& toupper();
    fastring& tolower();

    fastring upper() const {
        fastring s(*this);
        s.toupper();
        return s;
    }

    fastring lower() const {
        fastring s(*this);
        s.tolower();
        return s;
    }

    fastring& lshift(size_t n) {
        if (this->size() <= n) { this->clear(); return *this; }
        memmove(_p, _p + n, _size -= n);
        return *this;
    }

    void shrink() {
        if (_size + 1 < _cap) this->swap(fastring(*this));
    }

  private:
    void _construct_from_c_str(const char* s, size_t n) {
        _cap = n;
        _size = n - 1;
        _p = (char*) co::alloc(n); assert(_p);
        memcpy(_p, s, n);
    }

    fastring& _assign(const void* s, size_t n) {
        _size = n;
        if (n > 0) {
            this->reserve(n);
            memcpy(_p, s, n);
        }
        return *this;
    }

    bool _is_inside(const char* p) const {
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

inline fastring operator+(const fastring& a, const std::string& b) {
    return fastring(a.size() + b.size() + 1).append(a).append(b);
}

inline fastring operator+(const std::string& a, const fastring& b) {
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

inline bool operator==(const fastring& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return a.size() == 0 || memcmp(a.data(), b.data(), a.size()) == 0;
}

inline bool operator==(const std::string& a, const fastring& b) {
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
    if (a.size() < b.size()) {
        return a.size() == 0 || memcmp(a.data(), b.data(), a.size()) <= 0;
    } else {
        return memcmp(a.data(), b.data(), b.size()) < 0;
    }
}

inline bool operator<(const fastring& a, const std::string& b) {
    if (a.size() < b.size()) {
        return a.size() == 0 || memcmp(a.data(), b.data(), a.size()) <= 0;
    } else {
        return memcmp(a.data(), b.data(), b.size()) < 0;
    }
}

inline bool operator<(const std::string& a, const fastring& b) {
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

inline bool operator>(const fastring& a, const std::string& b) {
    if (a.size() > b.size()) {
        return b.size() == 0 || memcmp(a.data(), b.data(), b.size()) >= 0;
    } else {
        return memcmp(a.data(), b.data(), a.size()) > 0;
    }
}

inline bool operator>(const std::string& a, const fastring& b) {
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
