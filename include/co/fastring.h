#pragma once

#include "def.h"
#include "atomic.h"
#include "array.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <string>
#include <ostream>

class fastring {
  public:
    static const size_t npos = (size_t)-1;

    constexpr fastring() noexcept : _p(0) {}

    explicit fastring(size_t cap) {
        this->_Init(cap, 0);
    }

    fastring(const void* s, size_t n);
    fastring(const char* s);
    fastring(const std::string& s) : fastring(s.data(), s.size()) {}

    fastring(size_t n, char c) {
        this->_Init(n + 1, n);
        memset(_p->s, c, n);
    }

    fastring(char c, size_t n) : fastring(n, c) {}

    static fastring from_raw_buffer(char* p, size_t cap, size_t size) {
        _Mem* m;
        new (m = _Mem::alloc()) _Mem(cap, size, p);
        return *(fastring*)&m;
    }

    ~fastring() {
        if (!_p) return;
        this->_UnRef();
        _p = 0;
    }

    fastring(fastring&& s) noexcept : _p(s._p) {
        s._p = 0;
    }

    fastring(const fastring& s) : _p(s._p) {
        if (_p) this->_Ref();
    }

    fastring& operator=(fastring&& s) noexcept {
        if (_p) this->_UnRef();
        _p = s._p;
        s._p = 0;
        return *this;
    }

    fastring& operator=(const fastring& s) {
        if (&s != this) {
            if (_p) this->_UnRef();
            _p = s._p;
            if (_p) this->_Ref();
        }
        return *this;
    }

    fastring& operator=(const char* s);
    fastring& operator=(const std::string& s);

    const char* data() const {
        return _p ? _p->s : (const char*) &_p;
    }

    size_t size() const {
        return _p ? _p->size : 0;
    }

    bool empty() const {
        return this->size() == 0;
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

    void reserve(size_t n);

    // !! newly allocated memory is not initialized
    void resize(size_t n) {
        this->reserve(n);
        _p->size = n;
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

    fastring& append(char c);

    fastring& append(char c, size_t n);

    fastring& append(size_t n, char c) {
        return this->append(c, n);
    }

    fastring& append(const fastring& s);

    fastring& append(const std::string& s) {
        return this->_Append(s.data(), s.size());
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

    fastring clone() const {
        return _p ? fastring(_p->s, _p->size) : fastring();
    }

    fastring substr(size_t pos) const {
        if (pos >= this->size()) return fastring();
        return fastring(_p->s + pos, _p->size - pos);
    }

    fastring substr(size_t pos, size_t len) const {
        if (pos >= this->size()) return fastring();
        size_t n = _p->size - pos;
        return fastring(_p->s + pos, len < n ? len : n);
    }

    size_t find(char c) const {
        if (this->empty()) return npos;
        char* p = (char*) memchr(_p->s, c, _p->size);
        return p ? p - _p->s : npos;
    }

    size_t find(char c, size_t pos) const {
        if (pos >= this->size()) return npos;
        char* p = (char*) memchr(_p->s + pos, c, _p->size - pos);
        return p ? p - _p->s : npos;
    }

    size_t find(const char* s) const {
        if (this->empty()) return npos;
        const char* p = strstr(this->c_str(), s);
        return p ? p - _p->s : npos;
    }

    size_t find(const char* s, size_t pos) const {
        if (pos >= this->size()) return npos;
        const char* p = strstr(this->c_str() + pos, s);
        return p ? p - _p->s : npos;
    }

    size_t rfind(char c) const {
        if (this->empty()) return npos;
        const char* p = strrchr(this->c_str(), c);
        return p ? p - _p->s : npos;
    }

    size_t rfind(const char* s) const;

    size_t find_first_of(const char* s) const {
        if (this->empty()) return npos;
        size_t r = strcspn(this->c_str(), s);
        return _p->s[r] ? r : npos;
    }

    size_t find_first_not_of(const char* s) const {
        if (this->empty()) return npos;
        size_t r = strspn(this->c_str(), s);
        return _p->s[r] ? r : npos;
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
        return this->size() >= n && memcmp(_p->s + this->size() - n, s, n) == 0;
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
        return this->clone().toupper();
    }

    fastring lower() const {
        return this->clone().tolower();
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
    void _Init(size_t cap, size_t size) {
        new (_p = _Mem::alloc()) _Mem(cap, size, malloc(cap));
    }

    void _Ref() {
        atomic_inc(&_p->refn);
    }

    void _UnRef() {
        if (atomic_dec(&_p->refn) == 0) {
            free(_p->s);
            _Mem::dealloc(_p);
        }
    }

    void _Reserve(size_t n) {
        if (_p->cap < n) {
            _p->s = (char*) realloc(_p->s, n);
            assert(_p->s);
            _p->cap = n;
        }
    }

    void _Ensure(size_t n) {
        if (_p->cap < _p->size + n) {
            this->_Reserve((_p->cap * 3 >> 1) + n);
        }
    }

    bool _Inside(const char* p) const {
        return _p->s <= p && p < _p->s + _p->size;
    }

    fastring& _Append(const void* p, size_t n);
    fastring& _Append_safe(const void* p, size_t n);

  private:
    struct _Mem {
        _Mem(size_t c, size_t n, void* p)
            : cap(c), size(n), refn(1), s((char*)p) {
        }

        size_t cap;
        size_t size;
        size_t refn;
        char* s;

        static Array* pool() {
            static __thread Array* mp = 0;
            return mp ? mp : (mp = new Array(32));
        }

        static _Mem* alloc() {
            Array* mp = _Mem::pool();
            if (!mp->empty()) return (_Mem*) mp->pop_back();
            return (_Mem*) malloc(sizeof(_Mem));
        }

        static void dealloc(_Mem* p) {
            Array* mp = _Mem::pool();
            mp->size() < 32 * 1024 ? mp->push_back(p) : free(p);
        }
    };

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

inline std::ostream& operator<<(std::ostream& os, const fastring& s) {
    return os.write(s.data(), s.size());
}
