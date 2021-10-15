#pragma once

#include "def.h"
#include "__/dtoa_milo.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <new>
#include <type_traits>
#include <utility>

namespace fast {

// double to ascii string, return length of the result
inline int dtoa(double v, char* buf) {
    return milo::dtoa(v, buf);
}

// unsigned integer to hex string, return length of the result
//   - 255 -> "0xff"
__codec int u32toh(uint32 v, char* buf);
__codec int u64toh(uint64 v, char* buf);

// integer to ascii string, return length of the result
__codec int u32toa(uint32 v, char* buf);
__codec int u64toa(uint64 v, char* buf);

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

namespace xx {
template<bool> struct le_32bit{};

template<typename V>
inline int itoa(V v, char* buf, le_32bit<true>) {
    return i32toa((int32)v, buf);
}

template<typename V>
inline int itoa(V v, char* buf, le_32bit<false>) {
    return i64toa((int64)v, buf);
}

template<typename V>
inline int utoa(V v, char* buf, le_32bit<true>) {
    return u32toa((uint32)v, buf);
}

template<typename V>
inline int utoa(V v, char* buf, le_32bit<false>) {
    return u64toa((uint64)v, buf);
}

template<int>
int ptoh(const void* p, char* buf);

template<>
inline int ptoh<4>(const void* p, char* buf) {
    return u32toh((uint32)(size_t)p, buf);
}

template<>
inline int ptoh<8>(const void* p, char* buf) {
    return u64toh((uint64)p, buf);
}
} // xx

// signed integer to ascii string
template<typename V>
inline int itoa(V v, char* buf) {
    return xx::itoa(v, buf, xx::le_32bit<sizeof(V) <= sizeof(uint32)>());
}

// unsigned integer to ascii string
template<typename V>
inline int utoa(V v, char* buf) {
    return xx::utoa(v, buf, xx::le_32bit<sizeof(V) <= sizeof(uint32)>());
}

// pointer to hex string
inline int ptoh(const void* p, char* buf) {
    return xx::ptoh<sizeof(p)>(p, buf);
}

class __codec stream {
  public:
    constexpr stream() noexcept
        : _cap(0), _size(0), _p(0) {
    }
    
    explicit stream(size_t cap)
        : _cap(cap), _size(0), _p((char*) malloc(cap)) {
    }

    stream(void* p, size_t size, size_t cap) noexcept
        : _cap(cap), _size(size), _p((char*)p) {
    }

    ~stream() {
        if (_p) free(_p);
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
            if (_p) free(_p);
            new (this) stream(std::move(s));
        }
        return *this;
    }

    const char* data() const {
        return _p;
    }

    size_t size() const {
        return _size;
    }

    bool empty() const {
        return _size == 0;
    }

    size_t capacity() const {
        return _cap;
    }

    void clear() {
        _size = 0;
    }

    void safe_clear() {
        memset(_p, 0, _size);
        _size = 0;
    }

    void resize(size_t n) {
        this->reserve(n);
        _size = n;
    }

    void reserve(size_t n) {
        if (_cap < n) {
            _p = (char*) realloc(_p, n); assert(_p);
            _cap = n;
        }
    }

    void ensure(size_t n) {
        if (_cap < _size + n) {
            _cap += ((_cap >> 1) + n);
            _p = (char*) realloc(_p, _cap); assert(_p);
        }
    }

    const char* c_str() const {
        ((stream*)this)->reserve(_size + 1);
        if (_p[_size] != '\0') _p[_size] = '\0';
        return _p;
    }

    char& back() const {
        return _p[_size - 1];
    }

    char& front() const {
        return _p[0];
    }

    char& operator[](size_t i) const {
        return _p[i];
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

    stream& operator<<(const char* v) {
        return this->append(v, strlen(v));
    }

    stream& operator<<(const void* v) {
        this->ensure(sizeof(v) * 3);
        _size += fast::ptoh(v, _p + _size);
        return *this;
    }

    stream& operator<<(float v) {
        this->ensure(24);
        _size += fast::dtoa(v, _p + _size);
        return *this;
    }

    stream& operator<<(double v) {
        this->ensure(24);
        _size += fast::dtoa(v, _p + _size);
        return *this;
    }
    
    size_t _cap;
    size_t _size;
    char* _p;
};

} // namespace fast
