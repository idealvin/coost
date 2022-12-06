#pragma once

#include "def.h"
#include "god.h"
#include "mem.h"
#include "__/dtoa_milo.h"

#include <assert.h>
#include <string.h>
#include <new>

namespace co {
// max decimal places
struct maxdp {
    explicit constexpr maxdp(int n) noexcept : n(n) {}
    int n;
};
} // co

typedef co::maxdp maxdp_t;

namespace fast {

// double to ascii string, return length of the result
// @mdp  max decimal places
inline int dtoa(double v, char* buf, int mdp=324) {
    return milo::dtoa(v, buf, mdp);
}

// unsigned integer to hex string, return length of the result
//   - 255 -> "0xff"
__coapi int u32toh(uint32 v, char* buf);
__coapi int u64toh(uint64 v, char* buf);

// integer to ascii string, return length of the result
__coapi int u32toa(uint32 v, char* buf);
__coapi int u64toa(uint64 v, char* buf);

inline int i32toa(int32 v, char* buf) {
    if (v >= 0) return u32toa((uint32)v, buf);
    *buf = '-';
    return u32toa((uint32)(-v), buf + 1) + 1;
}

inline int i64toa(int64 v, char* buf) {
    if (v >= 0) return u64toa((uint64)v, buf);
    *buf = '-';
    return u64toa((uint64)(-v), buf + 1) + 1;
}

// signed integer to ascii string
template<typename V, god::enable_if_t<sizeof(V) <= sizeof(uint32), int> = 0>
inline int itoa(V v, char* buf) {
    return i32toa((int32)v, buf);
}

template<typename V, god::enable_if_t<(sizeof(V) > sizeof(uint32)), int> = 0>
inline int itoa(V v, char* buf) {
    return i64toa((int64)v, buf);
}

// unsigned integer to ascii string
template<typename V, god::enable_if_t<sizeof(V) <= sizeof(uint32), int> = 0>
inline int utoa(V v, char* buf) {
    return u32toa((uint32)v, buf);
}

template<typename V, god::enable_if_t<(sizeof(V) > sizeof(uint32)), int> = 0>
inline int utoa(V v, char* buf) {
    return u64toa((uint64)v, buf);
}

#if __arch64
// pointer to hex string
inline int ptoh(const void* p, char* buf) {
    return u64toh((uint64)p, buf);
}

#else
inline int ptoh(const void* p, char* buf) {
    return u32toh((uint32)(size_t)p, buf);
}
#endif

class __coapi stream {
  public:
    constexpr stream() noexcept
        : _cap(0), _size(0), _p(0) {
    }
    
    explicit stream(size_t cap)
        : _cap(cap), _size(0) {
        _p = (char*) co::alloc(cap); assert(_p);
    }

    stream(size_t cap, size_t size)
        : _cap(cap), _size(size) {
        _p = (char*) co::alloc(cap); assert(_p);
    }

    stream(void* p, size_t size, size_t cap) noexcept
        : _cap(cap), _size(size), _p((char*)p) {
    }

    ~stream() {
        if (_p) co::free(_p, _cap);
    }

    stream(const stream&) = delete;
    void operator=(const stream&) = delete;

    stream(stream&& s) noexcept
        : _cap(s._cap), _size(s._size), _p(s._p) {
        s._p = 0;
        s._cap = s._size = 0;
    }

    stream& operator=(stream&& s) {
        if (&s != this) {
            if (_p) co::free(_p, _cap);
            new (this) stream(std::move(s));
        }
        return *this;
    }

    const char* data() const noexcept { return _p; }
    size_t size() const noexcept { return _size; }
    bool empty() const noexcept { return _size == 0; }
    size_t capacity() const noexcept { return _cap; }
    void clear() noexcept { _size = 0; }

    // like clear(), but will fill the internal memory with zeros
    void safe_clear() {
        memset(_p, 0, _size);
        _size = 0;
    }

    const char* c_str() const {
        ((stream*)this)->reserve(_size + 1);
        if (_p[_size] != '\0') _p[_size] = '\0';
        return _p;
    }

    char& back() {
        assert(_size > 0);
        return _p[_size - 1];
    }

    const char& back() const {
        assert(_size > 0);
        return _p[_size - 1];
    }

    char& front() {
        assert(_size > 0);
        return _p[0];
    }

    const char& front() const {
        assert(_size > 0);
        return _p[0];
    }

    char& operator[](size_t i) {
        assert(i < _size);
        return _p[i];
    }

    const char& operator[](size_t i) const {
        assert(i < _size);
        return _p[i];
    }
    
    void resize(size_t n) {
        this->reserve(n);
        _size = n;
    }

    void safe_resize(size_t n) {
        if (_size < n) {
            this->reserve(n);
            memset(_p + _size, 0, n - _size);
        }
        _size = n;
    }

    void reserve(size_t n) {
        if (_cap < n) {
            _p = (char*) co::realloc(_p, _cap, n); assert(_p);
            _cap = n;
        }
    }

    void reset() {
        if (_p) { co::free(_p, _cap); _p = 0; }
        _cap = _size = 0;
    }

    void ensure(size_t n) {
        if (_cap < _size + n) {
            const size_t old_cap = _cap;
            _cap += ((_cap >> 1) + n);
            _p = (char*) co::realloc(_p, old_cap, _cap); assert(_p);
        }
    }

    void swap(stream& fs) noexcept {
        std::swap(fs._cap, _cap);
        std::swap(fs._size, _size);
        std::swap(fs._p, _p);
    }

    void swap(stream&& fs) noexcept {
        fs.swap(*this);
    }

  protected:
    stream& append(size_t n, char c) {
        this->ensure(n);
        memset(_p + _size, c, n);
        _size += n;
        return *this;
    }

    stream& append(char c) {
        this->ensure(1);
        _p[_size++] = c;
        return *this;
    }

    stream& append(const void* p, size_t n) {
        this->ensure(n);
        memcpy(_p + _size, p, n);
        _size += n;
        return *this;
    }

    stream& safe_append(const void* s, size_t n) {
        const char* const p = (const char*) s;
        if (!(_p <= p && p < _p + _size)) return this->append(p, n);

        assert(p + n <= _p + _size);
        const size_t pos = p - _p;
        this->ensure(n);
        memcpy(_p + _size, _p + pos, n);
        _size += n;
        return *this;
    }

    stream& operator<<(bool v) {
        return v ? this->append("true", 4) : this->append("false", 5);
    }

    stream& operator<<(char v) {
        return this->append(v);
    }

    stream& operator<<(signed char v) {
        return this->operator<<((char)v);
    }

    stream& operator<<(unsigned char v) {
        return this->operator<<((char)v);
    }

    stream& operator<<(short v) {
        this->ensure(sizeof(v) * 3);
        _size += fast::itoa(v, _p + _size);
        return *this;
    }

    stream& operator<<(unsigned short v) {
        this->ensure(sizeof(v) * 3);
        _size += fast::utoa(v, _p + _size);
        return *this;
    }

    stream& operator<<(int v) {
        this->ensure(sizeof(v) * 3);
        _size += fast::itoa(v, _p + _size);
        return *this;
    }

    stream& operator<<(unsigned int v) {
        this->ensure(sizeof(v) * 3);
        _size += fast::utoa(v, _p + _size);
        return *this;
    }

    stream& operator<<(long v) {
        this->ensure(sizeof(v) * 3);
        _size += fast::itoa(v, _p + _size);
        return *this;
    }

    stream& operator<<(unsigned long v) {
        this->ensure(sizeof(v) * 3);
        _size += fast::utoa(v, _p + _size);
        return *this;
    }

    stream& operator<<(long long v) {
        static_assert(sizeof(v) <= sizeof(int64), "");
        this->ensure(sizeof(v) * 3);
        _size += fast::itoa(v, _p + _size);
        return *this;
    }

    stream& operator<<(unsigned long long v) {
        static_assert(sizeof(v) <= sizeof(uint64), "");
        this->ensure(sizeof(v) * 3);
        _size += fast::utoa(v, _p + _size);
        return *this;
    }

    stream& operator<<(float v) {
        this->ensure(24);
        _size += fast::dtoa(v, _p + _size, 6);
        return *this;
    }

    stream& operator<<(double v) {
        this->ensure(24);
        _size += fast::dtoa(v, _p + _size, 6);
        return *this;
    }

    stream& operator<<(const char* s) {
        return this->append(s, strlen(s));
    }

    stream& operator<<(const void* v) {
        this->ensure(sizeof(v) * 3);
        _size += fast::ptoh(v, _p + _size);
        return *this;
    }

    stream& operator<<(std::nullptr_t) {
        return this->append("0x0", 3);
    }

    size_t _cap;
    size_t _size;
    char* _p;
};

} // namespace fast
