#pragma once

#include "def.h"
#include "dtoa.h"
#include "mem.h"
#include "murmur_hash.h"
#include <string.h>
#include <string>
#include <string_view>
#include <ostream>
#include <vector>
#include <charconv>
#include <type_traits>


namespace co {

// the buffer length should be 25 at least
inline int dtoa(double v, char* buf, int mdp=324) {
    return milo::dtoa(v, buf, mdp);
}

template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
inline int itoa(T v, char* buf, uint32 buf_size) {
    auto [p, _] = std::to_chars(buf, buf + buf_size, v, 10);
    return int(p - buf);
}

template<typename T,
    typename = std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>>>
inline int utoh(T v, char* buf, uint32 buf_size) {
    buf[0] = '0';
    buf[1] = 'x';
    auto [p, _] = std::to_chars(buf + 2, buf + buf_size, v, 16);
    return int(p - buf);
}

inline int ptoh(const void* v, char* buf, uint32 buf_size) {
    return utoh((size_t)v, buf, buf_size);
}

char* memrchr(const char* s, char c, size_t n);
char* memmem(const char* s, size_t n, const char* p, size_t m);
char* memimem(const char* s, size_t n, const char* p, size_t m);
char* memrmem(const char* s, size_t n, const char* p, size_t m);

inline int memcmp(const char* s, size_t n, const char* p, size_t m) {
    const int i = ::memcmp(s, p, n < m ? n : m);
    return i != 0 ? i : (n < m ? -1 : n != m);
}

struct decimal {
    constexpr decimal(double v, int n) noexcept : v(v), n(n) {}
    double v;
    int n; // significant decimal places
};

struct string {
    static const size_t npos = (size_t)-1;

    constexpr string() noexcept
        : _cap(0), _size(0), _p(0) {
    }
    
    explicit string(size_t cap) noexcept
        : _cap(cap), _size(0) {
        _p = cap > 0 ? _malloc(cap) : 0;
    }

    ~string() { this->reset(); }

    string(size_t n, char c) noexcept {
        this->_init(n + 1, n);
        ::memset(_p, c, n);
    }

    string(const void* s, size_t n) noexcept {
        this->_init(n + !!n, n);
        ::memcpy(_p, s, n);
    }

    string(const char* s) noexcept : string(s, s ? strlen(s) : 0) {}
    string(const string& s) noexcept : string(s.data(), s.size()) {}
    string(const std::string& s) noexcept : string(s.data(), s.size()) {}

    string(string&& s) noexcept
        : _cap(s._cap), _size(s._size), _p(s._p) {
        s._cap = s._size = 0;
        s._p = 0;
    }

    string& operator=(string&& s) noexcept {
        if (&s != this) {
            if (_p) _free(_p, _cap);
            new (this) string(std::move(s));
        }
        return *this;
    }

    string& operator=(const char* s) noexcept {
        return this->assign(s, strlen(s));
    }

    string& operator=(const string& s) noexcept {
        return &s != this ? this->_assign(s.data(), s.size()) : *this;
    }

    string& operator=(const std::string& s) noexcept {
        return this->_assign(s.data(), s.size());
    }

    string& assign(const void* s, size_t n) noexcept {
        if (!this->_inside((const char*)s)) return this->_assign(s, n);
        runtime_assert((const char*)s + n <= _p + _size);
        if (s != _p) ::memmove(_p, s, n);
        _size = n;
        return *this;
    }

    string& assign(size_t n, char c) noexcept {
        this->reserve(n + 1);
        ::memset(_p, c, n);
        _size = n;
        return *this;
    }

    template<typename S>
    string& assign(S&& s) noexcept {
        return this->operator=(std::forward<S>(s));
    }

    char* data() noexcept { return _p; }
    const char* data() const noexcept { return _p; }
    size_t size() const noexcept { return _size; }
    bool empty() const noexcept { return _size == 0; }
    size_t capacity() const noexcept { return _cap; }
    void clear() noexcept { _size = 0; }
    void zero_clear() noexcept;

    const char* c_str() const noexcept {
        if (_p) {
            runtime_assert(_size < _cap);
            if (_p[_size] != '\0') _p[_size] = '\0';
            return _p;
        }
        return "";
    }

    char& back() noexcept { return _p[_size - 1]; }
    const char& back() const noexcept { return _p[_size - 1]; }

    char& front() noexcept { return _p[0]; }
    const char& front() const noexcept { return _p[0]; }

    char& operator[](size_t i) noexcept { return _p[i]; }
    const char& operator[](size_t i) const noexcept { return _p[i]; }

    // resize only, will not fill the expanded memory with zeros
    void resize(size_t n) noexcept {
        this->reserve(n + 1);
        _size = n;
    }
   
    void reserve(size_t n) noexcept {
        if (_cap < n) {
            _p = _realloc(_p, _cap, n);
            _cap = n;
        }
    }

    void reset() noexcept {
        if (_p) {
            _free(_p, _cap);
            _p = 0;
            _cap = _size = 0;
        }
    }

    void ensure(size_t n) {
        if (_cap <= _size + n) {
            const size_t cap = _cap;
            _cap += ((_cap >> 1) + n + 1);
            _p = _realloc(_p, cap, _cap);
        }
    }

    void swap(string& s) noexcept {
        std::swap(s._cap, _cap);
        std::swap(s._size, _size);
        std::swap(s._p, _p);
    }

    void swap(string&& s) noexcept { s.swap(*this); }

    string& append(char c) noexcept {
        this->ensure(1);
        _p[_size++] = c;
        return *this;
    }

    string& append(size_t n, char c) noexcept {
        this->ensure(n);
        ::memset(_p + _size, c, n);
        _size += n;
        return *this;
    }

    string& append(const void* s, size_t n) noexcept {
        const char* const p = (const char*) s;
        if (!this->_inside(p)) return this->append_nomchk(p, n);

        const size_t pos = p - _p;
        runtime_assert(pos + n <= _size);
        this->ensure(n);
        ::memcpy(_p + _size, _p + pos, n);
        _size += n;
        return *this;
    }

    string& append(const char* s) noexcept {
        return this->append(s, strlen(s));
    }

    string& append(const string& s) noexcept {
        if (&s != this) return this->append_nomchk(s.data(), s.size());
        this->reserve((_size << 1) + !!_size);
        ::memcpy(_p + _size, _p, _size); // append itself
        _size <<= 1;
        return *this;
    }

    string& append(const std::string& s) noexcept {
        return this->append_nomchk(s.data(), s.size());
    }

    string& append_nomchk(const void* p, size_t n) noexcept {
        this->ensure(n);
        ::memcpy(_p + _size, p, n);
        _size += n;
        return *this;
    }

    string& append_nomchk(const char* s) noexcept {
        return this->append_nomchk(s, strlen(s));
    }

    string& operator+=(char c) noexcept {
        return this->append(c);
    }

    string& operator+=(const char* s) noexcept {
        return this->append(s);
    }

    string& operator+=(const string& s) noexcept {
        return this->append(s);
    }

    string& operator+=(const std::string& s) noexcept {
        return this->append(s);
    }

    string& push_back(char c) noexcept { return this->append(c); }

    char pop_back() noexcept { return _p[--_size]; }

    string& operator<<(bool v) noexcept {
        return v ? this->append_nomchk("true", 4) : this->append_nomchk("false", 5);
    }

    string& operator<<(char v) noexcept {
        return this->append(v);
    }

    string& operator<<(signed char v) noexcept {
        return this->operator<<((char)v);
    }

    string& operator<<(unsigned char v) noexcept {
        return this->operator<<((char)v);
    }

    string& operator<<(short v) noexcept {
        this->ensure(sizeof(v) * 3);
        _size += co::itoa(v, _p + _size, sizeof(v) * 3);
        return *this;
    }

    string& operator<<(unsigned short v) noexcept {
        this->ensure(sizeof(v) * 3);
        _size += co::itoa(v, _p + _size, sizeof(v) * 3);
        return *this;
    }

    string& operator<<(int v) noexcept {
        this->ensure(sizeof(v) * 3);
        _size += co::itoa(v, _p + _size, sizeof(v) * 3);
        return *this;
    }

    string& operator<<(unsigned int v) noexcept {
        this->ensure(sizeof(v) * 3);
        _size += co::itoa(v, _p + _size, sizeof(v) * 3);
        return *this;
    }

    string& operator<<(long v) noexcept {
        this->ensure(sizeof(v) * 3);
        _size += co::itoa(v, _p + _size, sizeof(v) * 3);
        return *this;
    }

    string& operator<<(unsigned long v) noexcept {
        this->ensure(sizeof(v) * 3);
        _size += co::itoa(v, _p + _size, sizeof(v) * 3);
        return *this;
    }

    string& operator<<(long long v) noexcept {
        static_assert(sizeof(v) <= sizeof(int64), "");
        this->ensure(sizeof(v) * 3);
        _size += co::itoa(v, _p + _size, sizeof(v) * 3);
        return *this;
    }

    string& operator<<(unsigned long long v) noexcept {
        static_assert(sizeof(v) <= sizeof(uint64), "");
        this->ensure(sizeof(v) * 3);
        _size += co::itoa(v, _p + _size, sizeof(v) * 3);
        return *this;
    }

    string& operator<<(double v) noexcept {
        this->ensure(25);
        _size += co::dtoa(v, _p + _size, 16);
        return *this;
    }

    string& operator<<(float v) noexcept {
        return this->operator<<((double)v);
    }

    string& operator<<(const decimal& v) noexcept {
        this->ensure(25);
        _size += co::dtoa(v.v, _p + _size, v.n);
        return *this;
    }

    string& operator<<(const void* v) noexcept {
        constexpr size_t N = sizeof(v) * 2 + 3;
        this->ensure(N);
        _size += co::ptoh(v, _p + _size, N);
        return *this;
    }

    string& operator<<(std::nullptr_t) noexcept {
        return this->append_nomchk("0x0", 3);
    }

    string& operator<<(const char* s) noexcept {
        return this->append(s, strlen(s));
    }

    string& operator<<(const signed char* s) noexcept {
        return this->operator<<((const char*)s);
    }

    string& operator<<(const unsigned char* s) noexcept {
        return this->operator<<((const char*)s);
    }

    string& operator<<(const string& s) noexcept {
        return this->append(s);
    }

    string& operator<<(const std::string& s) noexcept {
        return this->append_nomchk(s.data(), s.size());
    }

    string& operator<<(const std::string_view& s) noexcept {
        return this->append(s.data(), s.size());
    }

    string& cat() noexcept { return *this; }

    template<typename X, typename ...V>
    string& cat(X&& x, V&& ... v) noexcept {
        (*this) << std::forward<X>(x);
        return this->cat(std::forward<V>(v)...);
    }

    int compare(const char* s, size_t n) const noexcept {
        return co::memcmp(_p, _size, s, n);
    }

    int compare(const char* s) const noexcept {
        return this->compare(s, strlen(s));
    }

    int compare(const string& s) const noexcept {
        return this->compare(s.data(), s.size());
    }

    int compare(const std::string& s) const noexcept {
        return this->compare(s.data(), s.size());
    }

    bool contains(char c) const noexcept {
        return this->find(c) != npos;
    }

    bool contains(const char* s) const noexcept {
        return this->find(s) != npos;
    }

    bool contains(const string& s) const noexcept {
        return this->find(s) != npos;
    }

    bool contains(const std::string& s) const noexcept {
        return this->find(s) != npos;
    }

    bool starts_with(char c) const noexcept {
        return !this->empty() && this->front() == c;
    }

    bool starts_with(const char* s, size_t n) const noexcept {
        return n == 0 || (n <= _size && ::memcmp(_p, s, n) == 0);
    }

    bool starts_with(const char* s) const noexcept {
        return this->starts_with(s, strlen(s));
    }

    bool starts_with(const string& s) const noexcept {
        return this->starts_with(s.data(), s.size());
    }

    bool starts_with(const std::string& s) const noexcept {
        return this->starts_with(s.data(), s.size());
    }

    bool ends_with(char c) const noexcept {
        return !this->empty() && this->back() == c;
    }

    bool ends_with(const char* s, size_t n) const noexcept {
        return n == 0 || (n <= _size && ::memcmp(_p + _size - n, s, n) == 0);
    }

    bool ends_with(const char* s) const noexcept {
        return this->ends_with(s, strlen(s));
    }

    bool ends_with(const string& s) const noexcept {
        return this->ends_with(s.data(), s.size());
    }

    bool ends_with(const std::string& s) const noexcept {
        return this->ends_with(s.data(), s.size());
    }

    size_t find(char c) const noexcept {
        if (!this->empty()) {
            char* const p = (char*) ::memchr(_p, c, _size);
            return p ? p - _p : npos;
        }
        return npos;
    }

    size_t find(char c, size_t pos) const noexcept {
        if (pos < _size) {
            char* const p = (char*) ::memchr(_p + pos, c, _size - pos);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find char in substr(pos, len)
    size_t find(char c, size_t pos, size_t len) const noexcept {
        if (pos < _size) {
            const size_t n = _size - pos;
            char* const p = (char*) ::memchr(_p + pos, c, len < n ? len : n);
            return p ? p - _p : npos;
        }
        return npos;
    }

    size_t find(const char* s) const noexcept {
        char* const p = co::memmem(_p, _size, s, strlen(s));
        return p ? p - _p : npos;
    }

    // find @s (length: @n) from @pos
    size_t find(const char* s, size_t pos, size_t n) const noexcept {
        if (pos < _size) {
            char* const p = co::memmem(_p + pos, _size - pos, s, n);
            return p ? p - _p : npos;
        }
        return npos;
    }

    size_t find(const char* s, size_t pos) const noexcept {
        return this->find(s, pos, strlen(s));
    }

    size_t find(const string& s, size_t pos=0) const noexcept {
        return this->find(s.data(), pos, s.size());
    }

    size_t find(const std::string& s, size_t pos=0) const noexcept {
        return this->find(s.data(), pos, s.size());
    }

    size_t ifind(const char* s) const noexcept {
        char* const p = co::memimem(_p, _size, s, strlen(s));
        return p ? p - _p : npos;
    }

    // find @s (length: @n) from @pos (ignore the case)
    size_t ifind(const char* s, size_t pos, size_t n) const noexcept {
        if (pos < _size) {
            char* const p = co::memimem(_p + pos, _size - pos, s, n);
            return p ? p - _p : npos;
        }
        return npos;
    }

    size_t ifind(const char* s, size_t pos) const noexcept {
        return this->ifind(s, pos, strlen(s));
    }

    // find @s from @pos (ignore the case)
    size_t ifind(const string& s, size_t pos=0) const noexcept {
        return this->ifind(s.data(), pos, s.size());
    }

    // find @s from @pos (ignore the case)
    size_t ifind(const std::string& s, size_t pos=0) const noexcept {
        return this->ifind(s.data(), pos, s.size());
    }

    size_t ifind(char c, size_t pos=0) const noexcept {
        return this->ifind(&c, pos, 1);
    }

    size_t rfind(char c) const noexcept {
        char* const p = co::memrchr(_p, c, _size);
        return p ? p - _p : npos;
    }

    size_t rfind(char c, size_t pos) const noexcept {
        char* const p = co::memrchr(_p, c, pos < _size ? pos + 1 : _size);
        return p ? p - _p : npos;
    }

    size_t rfind(const char* s) const noexcept {
        const size_t n = strlen(s);
        if (n > 0) {
            char* const p = co::memrmem(_p, _size, s, n);
            return p ? p - _p : npos;
        }
        return _size;
    }

    // reverse find @s (length: @n) from @pos
    size_t rfind(const char* s, size_t pos, size_t n) const noexcept {
        if (n > 0) {
            char* const p = co::memrmem(_p, pos >= _size ? _size : pos + 1, s, n);
            return p ? p - _p : npos;
        }
        return pos >= _size ? _size : pos;
    }

    size_t rfind(const char* s, size_t pos) const noexcept {
        return this->rfind(s, pos, strlen(s));
    }

    size_t rfind(const string& s, size_t pos=npos) const noexcept {
        return this->rfind(s.data(), pos, s.size());
    }

    size_t rfind(const std::string& s, size_t pos=npos) const noexcept {
        return this->rfind(s.data(), pos, s.size());
    }

    // find first char in @s (length: @n) from @pos
    size_t find_first_of(const char* s, size_t pos, size_t n) const noexcept;

    size_t find_first_of(const char* s, size_t pos=0) const noexcept {
        return this->find_first_of(s, pos, strlen(s));
    }

    size_t find_first_of(const string& s, size_t pos=0) const noexcept {
        return this->find_first_of(s.data(), pos, s.size());
    }

    size_t find_first_of(const std::string& s, size_t pos=0) const noexcept {
        return this->find_first_of(s.data(), pos, s.size());
    }

    // find first char not in @s (length: @n) from @pos
    size_t find_first_not_of(const char* s, size_t pos, size_t n) const noexcept;

    size_t find_first_not_of(const char* s, size_t pos=0) const noexcept {
        return this->find_first_not_of(s, pos, strlen(s));
    }

    size_t find_first_not_of(const string& s, size_t pos=0) const noexcept {
        return this->find_first_not_of(s.data(), pos, s.size());
    }

    size_t find_first_not_of(const std::string& s, size_t pos=0) const noexcept {
        return this->find_first_not_of(s.data(), pos, s.size());
    }

    size_t find_first_not_of(char c, size_t pos=0) const noexcept;

    size_t find_last_of(const char* s, size_t pos, size_t n) const noexcept;

    size_t find_last_of(const char* s, size_t pos=npos) const noexcept {
        return this->find_last_of(s, pos, strlen(s));
    }

    size_t find_last_of(const string& s, size_t pos=npos) const noexcept {
        return this->find_last_of(s.data(), pos, s.size());
    }

    size_t find_last_of(const std::string& s, size_t pos=npos) const noexcept {
        return this->find_last_of(s.data(), pos, s.size());
    }

    size_t find_last_not_of(const char* s, size_t pos, size_t n) const noexcept;

    size_t find_last_not_of(const char* s, size_t pos=npos) const noexcept {
        return this->find_last_not_of(s, pos, strlen(s));
    }

    size_t find_last_not_of(const string& s, size_t pos=npos) const noexcept {
        return this->find_last_not_of(s.data(), pos, s.size());
    }

    size_t find_last_not_of(const std::string& s, size_t pos=npos) const noexcept {
        return this->find_last_not_of(s.data(), pos, s.size());
    }

    size_t find_last_not_of(char c, size_t pos=npos) const noexcept;

    // * for any chars, ? for single char
    bool match(const char* pattern) const noexcept;

    string& tolower() noexcept;
    string& toupper() noexcept;

    string lower() const noexcept {
        string s(*this); s.tolower(); return s;
    }

    string upper() const noexcept {
        string s(*this); s.toupper(); return s;
    }

    string substr(size_t pos) const noexcept {
        return pos < _size ? string(_p + pos, _size - pos) : string();
    }

    string substr(size_t pos, size_t len) const noexcept {
        if (pos < _size) {
            const size_t n = _size - pos;
            return string(_p + pos, len < n ? len : n);
        }
        return string();
    }

    // escape the following characters in string:
    //     '"', '\\', '\0', '\r', '\n', '\t', '\a', '\b', '\f', '\v'
    string& escape() noexcept;
    string& unescape() noexcept;

    // remove the leading @n characters and the trailing @n characters
    string& remove_outer(size_t n) noexcept;
    
    // remove the leading @n characters
    string& remove_prefix(size_t n) noexcept;
    string& remove_prefix(int n) noexcept { return remove_prefix((size_t)n); }

    // remove the trailing @n characters
    string& remove_suffix(size_t n) noexcept {
        _size = n < _size ? _size - n : 0;
        return *this;
    }

    string& remove_suffix(int n) noexcept {
        return remove_suffix((size_t)n);
    }

    string& remove_prefix(const char* s, size_t n) noexcept {
        return this->starts_with(s, n) ? this->remove_prefix(n) : *this;
    }

    string& remove_prefix(const char* s) noexcept {
        return this->remove_prefix(s, strlen(s));
    }

    string& remove_prefix(const string& s) noexcept {
        return this->remove_prefix(s.data(), s.size());
    }

    string& remove_prefix(const std::string& s) noexcept {
        return this->remove_prefix(s.data(), s.size());
    }

    string& remove_suffix(const char* s, size_t n) noexcept {
        return this->ends_with(s, n) ? this->remove_suffix(n) : *this;
    }

    string& remove_suffix(const char* s) noexcept {
        return this->remove_suffix(s, strlen(s));
    }

    string& remove_suffix(const string& s) noexcept {
        return this->remove_suffix(s.data(), s.size());
    }

    string& remove_suffix(const std::string& s) noexcept {
        return this->remove_suffix(s.data(), s.size());
    }

    // remove all leading and trailing character @c
    string& trim(char c) noexcept;

    // remove all leading character @c
    string& trim_left(char c) noexcept;

    // remove all trailing character @c
    string& trim_right(char c) noexcept;

    // remove all leading and trailing characters in @s
    string& trim(const char* s=" \t\r\n") noexcept;

    // remove all leading characters in @s
    string& trim_left(const char* s=" \t\r\n") noexcept;

    // remove all trailing characters in @s
    string& trim_right(const char* s=" \t\r\n") noexcept;

    // replace @sub with @to
    // @t: times, unlimited by default
    string& replace(const char* sub, size_t n, const char* to, size_t m, size_t t=0) noexcept;

    string& replace(const char* sub, const char* to, size_t t=0) noexcept {
        return this->replace(sub, strlen(sub), to, strlen(to), t);
    }

    string& replace(const string& sub, const string& to, size_t t=0) noexcept {
        return this->replace(sub.data(), sub.size(), to.data(), to.size(), t);
    }

    void shrink_to_fit() noexcept {
        if (_size + 1 < _cap) this->swap(string(*this));
    }

private:
    string& _assign(const void* s, size_t n) noexcept {
        _size = n;
        if (n > 0) {
            this->reserve(n + 1);
            ::memcpy(_p, s, n);
        }
        return *this;
    }

    void _init(size_t cap, size_t size) noexcept {
        _cap = cap;
        _size = size;
        _p = cap > 0 ? _malloc(cap) : 0;
    }

    bool _inside(const char* p) const noexcept {
        return _p <= p && p < _p + _size;
    }

    static char* _malloc(size_t n) noexcept {
        char* const x = (char*) co::alloc(n);
        runtime_assert(x);
        return x;
    }

    static char* _realloc(void* p, size_t o, size_t n) noexcept {
        char* const x = (char*) co::realloc(p, o, n);
        runtime_assert(x);
        return x;
    }

    static void _free(void* p, size_t n) noexcept {
        co::free(p, n);
    }

    size_t _cap;
    size_t _size;
    char* _p;
};


inline string to_string(bool v) noexcept {
    return v ? string("true", 4) : string("false", 5);
}

inline string to_string(int v) noexcept {
    string s(16);
    s << v;
    return s;
}

inline string to_string(unsigned int v) noexcept {
    string s(16);
    s << v;
    return s;
}

inline string to_string(long v) noexcept {
    string s(4 * sizeof(long));
    s << v;
    return s;
}

inline string to_string(unsigned long v) noexcept {
    string s(4 * sizeof(unsigned long));
    s << v;
    return s;
}

inline string to_string(long long v) noexcept {
    string s(4 * sizeof(long long));
    s << v;
    return s;
}

inline string to_string(unsigned long long v) noexcept {
    string s(4 * sizeof(unsigned long long));
    s << v;
    return s;
}

inline string to_string(double v) noexcept {
    string s(32);
    s << v;
    return s;
}

inline string to_string(float v) noexcept {
    return to_string(static_cast<double>(v));
}

int32 stoi32(const char* s, int* err=nullptr) noexcept;

inline int32 stoi32(const string& s, int* err=nullptr) noexcept {
    return stoi32(s.c_str(), err);
}

inline int32 stoi32(const std::string& s, int* err=nullptr) noexcept {
    return stoi32(s.c_str(), err);
}

int64 stoi64(const char* s, int* err=nullptr) noexcept;

inline int64 stoi64(const string& s, int* err=nullptr) noexcept {
    return stoi64(s.c_str(), err);
}

inline int64 stoi64(const std::string& s, int* err=nullptr) noexcept {
    return stoi64(s.c_str(), err);
}

uint32 stou32(const char* s, int* err=nullptr) noexcept;

inline uint32 stou32(const string& s, int* err=nullptr) noexcept {
    return stou32(s.c_str(), err);
}

inline uint32 stou32(const std::string& s, int* err=nullptr) noexcept {
    return stou32(s.c_str(), err);
}

uint64 stou64(const char* s, int* err=nullptr) noexcept;

inline uint64 stou64(const string& s, int* err=nullptr) noexcept {
    return stou64(s.c_str(), err);
}

inline uint64 stou64(const std::string& s, int* err=nullptr) noexcept {
    return stou64(s.c_str(), err);
}

inline int stoi(const char* s, int* err=nullptr) noexcept {
    static_assert(sizeof(int) == sizeof(int32), "");
    return stoi32(s, err);
}

inline int stoi(const string& s, int* err=nullptr) noexcept {
    return stoi(s.c_str(), err);
}

inline int stoi(const std::string& s, int* err=nullptr) noexcept {
    return stoi(s.c_str(), err);
}

bool stob(const char* s, int* err=nullptr) noexcept;

inline bool stob(const string& s, int* err=nullptr) noexcept {
    return stob(s.c_str(), err);
}

inline bool stob(const std::string& s, int* err=nullptr) noexcept {
    return stob(s.c_str(), err);
}

double stod(const char* s, int* err=nullptr) noexcept;

inline double stod(const string& s, int* err=nullptr) noexcept {
    return stod(s.c_str(), err);
}

inline double stod(const std::string& s, int* err=nullptr) noexcept {
    return stod(s.c_str(), err);
}

string replace(
    const char* s, size_t n,
    const char* sub, size_t m,
    const char* to, size_t l,
    size_t t=0
) noexcept;

inline string replace(
    const char* s, const char* sub, const char* to, size_t t=0) noexcept {
    return replace(s, strlen(s), sub, strlen(sub), to, strlen(to), t);
}

template<class T, class Alloc = co::stl_allocator<T>>
using vector = std::vector<T, Alloc>;

vector<string> split(const char* s, size_t n, char c, size_t t=0);

vector<string> split(const char* s, size_t n, const char* c, size_t m, size_t t=0);

// co::split("|x|y|", '|');    ->  [ "", "x", "y" ]
// co::split("xooy", 'o');     ->  [ "x", "", "y" ]
// co::split("xooy", 'o', 1);  ->  [ "x", "oy" ]
inline vector<string> split(const char* s, char c, size_t t=0) {
    return split(s, strlen(s), c, t);
}

inline vector<string> split(const string& s, char c, size_t t=0) {
    return split(s.data(), s.size(), c, t);
}

inline vector<string> split(const char* s, const char* c, size_t t=0) {
    return split(s, strlen(s), c, strlen(c), t);
}

inline vector<string> split(const string& s, const char* c, size_t t=0) {
    return split(s.data(), s.size(), c, strlen(c), t);
}

inline string remove_outer(const char* s, size_t n) noexcept {
    string x(s); x.remove_outer(n); return x;
}

inline string remove_outer(const string& s, size_t n) noexcept {
    string x(s); x.remove_outer(n); return x;
}

inline string remove_prefix(const char* s, size_t n) noexcept {
    string x(s); x.remove_prefix(n); return x;
}

inline string remove_prefix(const string& s, size_t n) noexcept {
    string x(s); x.remove_prefix(n); return x;
}

inline string remove_suffix(const char* s, size_t n) noexcept {
    string x(s); x.remove_suffix(n); return x;
}

inline string remove_suffix(const string& s, size_t n) noexcept {
    string x(s); x.remove_suffix(n); return x;
}

inline string remove_prefix(const char* s, const char* c) noexcept {
    string x(s); x.remove_prefix(c); return x;
}

inline string remove_prefix(const string& s, const char* c) noexcept {
    string x(s); x.remove_prefix(c); return x;
}

inline string remove_suffix(const char* s, const char* c) noexcept {
    string x(s); x.remove_suffix(c); return x;
}

inline string remove_suffix(const string& s, const char* c) noexcept {
    string x(s); x.remove_suffix(c); return x;
}

inline string trim(const char* s, const char* c=" \t\r\n") noexcept {
    string x(s); x.trim(c); return x;
}

inline string trim(const string& s, const char* c=" \t\r\n") noexcept {
    string x(s); x.trim(c); return x;
}

inline string trim(const char* s, char c) noexcept {
    string x(s); x.trim(c); return x;
}

inline string trim(const string& s, char c) noexcept {
    string x(s); x.trim(c); return x;
}

inline string trim_left(const char* s, const char* c=" \t\r\n") noexcept {
    string x(s); x.trim_left(c); return x;
}

inline string trim_left(const string& s, const char* c=" \t\r\n") noexcept {
    string x(s); x.trim_left(c); return x;
}

inline string trim_left(const char* s, char c) noexcept {
    string x(s); x.trim_left(c); return x;
}

inline string trim_left(const string& s, char c) noexcept {
    string x(s); x.trim_left(c); return x;
}

inline string trim_right(const char* s, const char* c=" \t\r\n") noexcept {
    string x(s); x.trim_right(c); return x;
}

inline string trim_right(const string& s, const char* c=" \t\r\n") noexcept {
    string x(s); x.trim_right(c); return x;
}

inline string trim_right(const char* s, char c) noexcept {
    string x(s); x.trim_right(c); return x;
}

inline string trim_right(const string& s, char c) noexcept {
    string x(s); x.trim_right(c); return x;
}

inline co::string operator+(const co::string& a, char b) noexcept {
    co::string s(a.size() + 2);
    s.append(a).append(b);
    return s;
}

inline co::string operator+(char a, const co::string& b) noexcept {
    co::string s(b.size() + 2);
    s.append(a).append(b);
    return s;
}

inline co::string operator+(const co::string& a, const co::string& b) noexcept {
    co::string s(a.size() + b.size() + 1);
    s.append(a).append(b);
    return s;
}

inline co::string operator+(const co::string& a, const std::string& b) noexcept {
    co::string s(a.size() + b.size() + 1);
    s.append(a).append(b);
    return s;
}

inline co::string operator+(const std::string& a, const co::string& b) noexcept {
    co::string s(a.size() + b.size() + 1);
    s.append(a).append(b);
    return s;
}

inline co::string operator+(const co::string& a, const char* b) noexcept {
    const size_t n = strlen(b);
    co::string s(a.size() + n + 1);
    s.append(a).append(b, n);
    return s;
}

inline co::string operator+(const char* a, const co::string& b) noexcept {
    const size_t n = strlen(a);
    co::string s(b.size() + n + 1);
    s.append(a, n).append(b);
    return s;
}

inline bool operator==(const co::string& a, const co::string& b) noexcept {
    return a.compare(b) == 0;
}

inline bool operator==(const co::string& a, const std::string& b) noexcept {
    return a.compare(b) == 0;
}

inline bool operator==(const std::string& a, const co::string& b) noexcept {
    return b == a;
}

inline bool operator==(const co::string& a, const char* b) noexcept {
    return a.compare(b) == 0;
}

inline bool operator==(const char* a, const co::string& b) noexcept {
    return b == a;
}

inline bool operator!=(const co::string& a, const co::string& b) noexcept {
    return !(a == b);
}

inline bool operator!=(const co::string& a, const std::string& b) noexcept {
    return !(a == b);
}

inline bool operator!=(const std::string& a, const co::string& b) noexcept {
    return !(a == b);
}

inline bool operator!=(const co::string& a, const char* b) noexcept {
    return !(a == b);
}

inline bool operator!=(const char* a, const co::string& b) noexcept {
    return b != a;
}

inline bool operator<(const co::string& a, const co::string& b) noexcept {
    return a.compare(b) < 0;
}

inline bool operator<(const co::string& a, const std::string& b) noexcept {
    return a.compare(b) < 0;
}

inline bool operator<(const std::string& a, const co::string& b) noexcept {
    return b.compare(a) > 0;
}

inline bool operator<(const co::string& a, const char* b) noexcept {
    return a.compare(b) < 0;
}

inline bool operator>(const co::string& a, const co::string& b) noexcept {
    return a.compare(b) > 0;
}

inline bool operator>(const co::string& a, const std::string& b) noexcept {
    return a.compare(b) > 0;
}

inline bool operator>(const std::string& a, const co::string& b) noexcept {
    return b.compare(a) < 0;
}

inline bool operator>(const co::string& a, const char* b) noexcept {
    return a.compare(b) > 0;
}

inline bool operator<(const char* a, const co::string& b) noexcept {
    return b > a;
}

inline bool operator>(const char* a, const co::string& b) noexcept {
    return b < a;
}

inline bool operator<=(const co::string& a, const co::string& b) noexcept {
    return !(a > b);
}

inline bool operator<=(const co::string& a, const std::string& b) noexcept {
    return !(a > b);
}

inline bool operator<=(const std::string& a, const co::string& b) noexcept {
    return !(a > b);
}

inline bool operator<=(const co::string& a, const char* b) noexcept {
    return !(a > b);
}

inline bool operator<=(const char* a, const co::string& b) noexcept {
    return !(b < a);
}

inline bool operator>=(const co::string& a, const co::string& b) noexcept {
    return !(a < b);
}

inline bool operator>=(const co::string& a, const std::string& b) noexcept {
    return !(a < b);
}

inline bool operator>=(const std::string& a, const co::string& b) noexcept {
    return !(a < b);
}

inline bool operator>=(const co::string& a, const char* b) noexcept {
    return !(a < b);
}

inline bool operator>=(const char* a, const co::string& b) noexcept {
    return !(b > a);
}

inline std::ostream& operator<<(std::ostream& os, const co::string& s) noexcept {
    return os.write(s.data(), s.size());
}

} // co

namespace std {

template<>
struct hash<co::string> {
    size_t operator()(const co::string& s) const noexcept {
        return co::murmur_hash(s.data(), s.size());
    }
};

} // std
