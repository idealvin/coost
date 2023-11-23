#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4706) // if ((a = x))
#endif

#include "fast.h"
#include "hash/murmur_hash.h"
#include <string>
#include <ostream>

namespace str {
__coapi char* memrchr(const char* s, char c, size_t n);
__coapi char* memmem(const char* s, size_t n, const char* p, size_t m);
__coapi char* memimem(const char* s, size_t n, const char* p, size_t m);
__coapi char* memrmem(const char* s, size_t n, const char* p, size_t m);
__coapi bool match(const char* s, size_t n, const char* p, size_t m);

inline int memcmp(const char* s, size_t n, const char* p, size_t m) {
    const int i = ::memcmp(s, p, n < m ? n : m);
    return i != 0 ? i : (n < m ? -1 : n != m);
}
} // str

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

    fastring(size_t n, char c)
        : fast::stream(n + 1, n) {
        memset(_p, c, n);
    }

    fastring(char* p, size_t cap, size_t size)
        : fast::stream(p, cap, size) {
    }

    fastring(const void* s, size_t n)
        : fast::stream(n + !!n, n) {
        memcpy(_p, s, n);
    }

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

    fastring& assign(const void* s, size_t n) {
        if (!this->_inside((const char*)s)) return this->_assign(s, n);
        assert((const char*)s + n <= _p + _size);
        if (s != _p) memmove(_p, s, n);
        _size = n;
        return *this;
    }

    fastring& assign(size_t n, char c) {
        this->reserve(n + 1);
        memset(_p, c, n);
        _size = n;
        return *this;
    }

    template<typename S>
    fastring& assign(S&& s) {
        return this->operator=(std::forward<S>(s));
    }

    fastring& append(const void* p, size_t n) {
        return (fastring&) fast::stream::append(p, n);
    }
 
    // like append(), but will not check if p overlaps with the internal memory
    fastring& append_nomchk(const void* p, size_t n) {
        return (fastring&) fast::stream::append_nomchk(p, n);
    }

    fastring& append(const char* s) {
        return this->append(s, strlen(s));
    }

    // like append(), but will not check if s overlaps with the internal memory
    fastring& append_nomchk(const char* s) {
        return this->append_nomchk(s, strlen(s));
    }

    fastring& append(const fastring& s) {
        if (&s != this) return this->append_nomchk(s.data(), s.size());
        this->reserve((_size << 1) + !!_size);
        memcpy(_p + _size, _p, _size); // append itself
        _size <<= 1;
        return *this;
    }

    fastring& append(const std::string& s) {
        return this->append_nomchk(s.data(), s.size());
    }

    fastring& append(size_t n, char c) {
        return (fastring&) fast::stream::append(n, c);
    }

    fastring& append(char c) {
        return (fastring&) fast::stream::append(c);
    }

    fastring& push_back(char c) { return this->append(c); }

    char pop_back() { return _p[--_size]; }

    fastring& operator+=(const fastring& s) {
        return this->append(s);
    }

    fastring& operator+=(const std::string& s) {
        return this->append(s);
    }

    fastring& operator+=(const char* s) {
        return this->append(s);
    }

    fastring& operator+=(char c) {
        return this->append(c);
    }

    fastring& cat() { return *this; }

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
        return this->operator<<((char)v);
    }

    fastring& operator<<(unsigned char v) {
        return this->operator<<((char)v);
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
        return this->append_nomchk(s.data(), s.size());
    }

    int compare(const char* s, size_t n) const {
        return str::memcmp(_p, _size, s, n);
    }

    int compare(const char* s) const {
        return this->compare(s, strlen(s));
    }

    int compare(const fastring& s) const noexcept {
        return this->compare(s.data(), s.size());
    }

    int compare(const std::string& s) const noexcept {
        return this->compare(s.data(), s.size());
    }

    int compare(size_t pos, size_t len, const char* s, size_t n) const {
        const intptr_t x = (intptr_t)(_size - pos);
        if (x > 0) return str::memcmp(_p + pos, len < (size_t)x ? len : x, s, n);
        return str::memcmp(_p, 0, s, n);
    }

    int compare(size_t pos, size_t len, const char* s) const {
        return this->compare(pos, len, s, strlen(s));
    }

    int compare(size_t pos, size_t len, const fastring& s) const {
        return this->compare(pos, len, s.data(), s.size());
    }

    int compare(size_t pos, size_t len, const std::string& s) const {
        return this->compare(pos, len, s.data(), s.size());
    }

    int compare(size_t pos, size_t len, const fastring& s, size_t spos, size_t n=npos) const {
        const intptr_t x = (intptr_t)(s.size() - spos);
        if (x > 0) return this->compare(pos, len, s.data() + spos, n < (size_t)x ? n : x);
        return this->compare(pos, len, s.data(), 0);
    }

    int compare(size_t pos, size_t len, const std::string& s, size_t spos, size_t n=npos) const {
        const intptr_t x = (intptr_t)(s.size() - spos);
        if (x > 0) return this->compare(pos, len, s.data() + spos, n < (size_t)x ? n : x);
        return this->compare(pos, len, s.data(), 0);
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
        return n == 0 || (n <= _size && ::memcmp(_p, s, n) == 0);
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
        return n == 0 || (n <= _size && ::memcmp(_p + _size - n, s, n) == 0);
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

    // remove character @c at the left or right side, or both sides
    // @d: 'l' or 'L' for left, 'r' or 'R' for right, otherwise for both sides
    fastring& trim(char c, char d='b');

    fastring& trim(unsigned char c, char d='b') {
        return this->trim((char)c, d);
    }

    fastring& trim(signed char c, char d='b') {
        return this->trim((char)c, d);
    }

    // remove characters in @s at the left or right side, or both sides
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

    // replace substring @sub (len: @n) with @to (len: @m)
    // try @t times at most
    fastring& replace(const char* sub, size_t n, const char* to, size_t m, size_t t=0);

    fastring& replace(const char* sub, const char* to, size_t t=0) {
        return this->replace(sub, strlen(sub), to, strlen(to), t);
    }

    fastring& replace(const fastring& sub, const fastring& to, size_t t=0) {
        return this->replace(sub.data(), sub.size(), to.data(), to.size(), t);
    }

    fastring& tolower();
    fastring& toupper();

    fastring lower() const {
        fastring s(*this); s.tolower(); return s;
    }

    fastring upper() const {
        fastring s(*this); s.toupper(); return s;
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

    // find char @c
    size_t find(char c) const {
        if (!this->empty()) {
            char* const p = (char*) memchr(_p, c, _size);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find char @c from @pos
    size_t find(char c, size_t pos) const {
        if (pos < _size) {
            char* const p = (char*) memchr(_p + pos, c, _size - pos);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find char @c in [pos, pos + len)
    size_t find(char c, size_t pos, size_t len) const {
        if (pos < _size) {
            const size_t n = _size - pos;
            char* const p = (char*) memchr(_p + pos, c, len < n ? len : n);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find sub string @s
    size_t find(const char* s) const {
        char* const p = str::memmem(_p, _size, s, strlen(s));
        return p ? p - _p : npos;
    }

    // find @s (length: @n) from @pos
    size_t find(const char* s, size_t pos, size_t n) const {
        if (pos < _size) {
            char* const p = str::memmem(_p + pos, _size - pos, s, n);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find @s from @pos
    size_t find(const char* s, size_t pos) const {
        return this->find(s, pos, strlen(s));
    }

    // find @s from @pos
    size_t find(const fastring& s, size_t pos=0) const {
        return this->find(s.data(), pos, s.size());
    }

    // find @s from @pos
    size_t find(const std::string& s, size_t pos=0) const {
        return this->find(s.data(), pos, s.size());
    }

    // find @s (ignore the case)
    size_t ifind(const char* s) const {
        char* const p = str::memimem(_p, _size, s, strlen(s));
        return p ? p - _p : npos;
    }

    // find @s (length: @n) from @pos (ignore the case)
    size_t ifind(const char* s, size_t pos, size_t n) const {
        if (pos < _size) {
            char* const p = str::memimem(_p + pos, _size - pos, s, n);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find @s from @pos (ignore the case)
    size_t ifind(const char* s, size_t pos) const {
        return this->ifind(s, pos, strlen(s));
    }

    // find @s from @pos (ignore the case)
    size_t ifind(const fastring& s, size_t pos=0) const {
        return this->ifind(s.data(), pos, s.size());
    }

    // find @s from @pos (ignore the case)
    size_t ifind(const std::string& s, size_t pos=0) const {
        return this->ifind(s.data(), pos, s.size());
    }

    // find char @c from @pos (ignore the case)
    size_t ifind(char c, size_t pos=0) const {
        return this->ifind(&c, pos, 1);
    }

    // reverse find char @c
    size_t rfind(char c) const {
        char* const p = str::memrchr(_p, c, _size);
        return p ? p - _p : npos;
    }

    // reverse find char @c from @pos
    size_t rfind(char c, size_t pos) const {
        char* const p = str::memrchr(_p, c, pos < _size ? pos + 1 : _size);
        return p ? p - _p : npos;
    }

    // reverse find sub string @s
    size_t rfind(const char* s) const {
        const size_t n = strlen(s);
        if (n > 0) {
            char* const p = str::memrmem(_p, _size, s, n);
            return p ? p - _p : npos;
        }
        return _size;
    }

    // reverse find @s (length: @n) from @pos
    size_t rfind(const char* s, size_t pos, size_t n) const {
        if (n > 0) {
            char* const p = str::memrmem(_p, pos >= _size ? _size : pos + 1, s, n);
            return p ? p - _p : npos;
        }
        return pos >= _size ? _size : pos;
    }

    // reverse find @s from @pos
    size_t rfind(const char* s, size_t pos) const {
        return this->rfind(s, pos, strlen(s));
    }

    // reverse find @s from @pos
    size_t rfind(const fastring& s, size_t pos=npos) const {
        return this->rfind(s.data(), pos, s.size());
    }

    // reverse find @s from @pos
    size_t rfind(const std::string& s, size_t pos=npos) const {
        return this->rfind(s.data(), pos, s.size());
    }

    // find first char in @s (length: @n) from @pos
    size_t find_first_of(const char* s, size_t pos, size_t n) const;

    // find first char in @s from @pos
    size_t find_first_of(const char* s, size_t pos=0) const {
        return this->find_first_of(s, pos, strlen(s));
    }

    // find first char in @s from @pos
    size_t find_first_of(const fastring& s, size_t pos=0) const {
        return this->find_first_of(s.data(), pos, s.size());
    }

    // find first char in @s from @pos
    size_t find_first_of(const std::string& s, size_t pos=0) const {
        return this->find_first_of(s.data(), pos, s.size());
    }

    // find first char not in @s (length: @n) from @pos
    size_t find_first_not_of(const char* s, size_t pos, size_t n) const;

    // find first char not in @s from @pos
    size_t find_first_not_of(const char* s, size_t pos=0) const {
        return this->find_first_not_of(s, pos, strlen(s));
    }

    // find first char not in @s from @pos
    size_t find_first_not_of(const fastring& s, size_t pos=0) const {
        return this->find_first_not_of(s.data(), pos, s.size());
    }

    // find first char not in @s from @pos
    size_t find_first_not_of(const std::string& s, size_t pos=0) const {
        return this->find_first_not_of(s.data(), pos, s.size());
    }

    // find first char not equal to @c
    size_t find_first_not_of(char c, size_t pos=0) const;

    // find last char in @s (length: @n) from @pos
    size_t find_last_of(const char* s, size_t pos, size_t n) const;

    // find last char in @s from @pos
    size_t find_last_of(const char* s, size_t pos=npos) const {
        return this->find_last_of(s, pos, strlen(s));
    }

    // find last char in @s from @pos
    size_t find_last_of(const fastring& s, size_t pos=npos) const {
        return this->find_last_of(s.data(), pos, s.size());
    }

    // find last char in @s from @pos
    size_t find_last_of(const std::string& s, size_t pos=npos) const {
        return this->find_last_of(s.data(), pos, s.size());
    }

    // find last char not in @s (length: @n) from @pos
    size_t find_last_not_of(const char* s, size_t pos, size_t n) const;

    // find last char not in @s from @pos
    size_t find_last_not_of(const char* s, size_t pos=npos) const {
        return this->find_last_not_of(s, pos, strlen(s));
    }

    // find last char not in @s from @pos
    size_t find_last_not_of(const fastring& s, size_t pos=npos) const {
        return this->find_last_not_of(s.data(), pos, s.size());
    }

    // find last char not in @s from @pos
    size_t find_last_not_of(const std::string& s, size_t pos=npos) const {
        return this->find_last_not_of(s.data(), pos, s.size());
    }

    // find last char not equal to @c
    size_t find_last_not_of(char c, size_t pos=npos) const;

    // * matches 0 or more characters
    // ? matches exactly one character
    bool match(const char* pattern) const {
        return str::match(_p, _size, pattern, strlen(pattern));
    }

    void shrink() {
        if (_size + 1 < _cap) this->swap(fastring(*this));
    }

  private:
    fastring& _assign(const void* s, size_t n) {
        _size = n;
        if (n > 0) {
            this->reserve(n + 1);
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
    return a.compare(b) == 0;
}

inline bool operator==(const fastring& a, const std::string& b) {
    return a.compare(b) == 0;
}

inline bool operator==(const std::string& a, const fastring& b) {
    return b == a;
}

inline bool operator==(const fastring& a, const char* b) {
    return a.compare(b) == 0;
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
    return a.compare(b) < 0;
}

inline bool operator<(const fastring& a, const std::string& b) {
    return a.compare(b) < 0;
}

inline bool operator<(const std::string& a, const fastring& b) {
    return b.compare(a) > 0;
}

inline bool operator<(const fastring& a, const char* b) {
    return a.compare(b) < 0;
}

inline bool operator>(const fastring& a, const fastring& b) {
    return a.compare(b) > 0;
}

inline bool operator>(const fastring& a, const std::string& b) {
    return a.compare(b) > 0;
}

inline bool operator>(const std::string& a, const fastring& b) {
    return b.compare(a) < 0;
}

inline bool operator>(const fastring& a, const char* b) {
    return a.compare(b) > 0;
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
