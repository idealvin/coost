#pragma once

#ifdef _WIN32
#pragma warning (disable:4624)
#endif

#include "fast.h"
#include "fastring.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <new>

class fastream {
  public:
    explicit fastream(size_t capacity = 32) {
        _cap = capacity;
        _size = 0;
        _p = (char*) malloc(capacity);
    }

    ~fastream() {
        free(_p);
    }

    fastream(const fastream&) = delete;
    void operator=(const fastream&) = delete;

    fastream(fastream&& fs) {
        memcpy(this, &fs, sizeof(fs));
        fs._p = 0;
    }

    fastream& operator=(fastream&& fs) {
        free(_p);
        memcpy(this, &fs, sizeof(fs));
        fs._p = 0;
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

    char& back() const {
        return _p[_size - 1];
    }

    char& front() const {
        return _p[0];
    }

    char& operator[](size_t i) const {
        return _p[i];
    }

    const char* c_str() const {
        ((fastream*)this)->reserve(_size + 1);
        if (_p[_size] != '\0') _p[_size] = '\0';
        return _p;
    }

    fastring str() const {
        return fastring(_p, _size);
    }

    void clear() {
        _size = 0;
    }

    void resize(size_t n) {
        this->reserve(n);
        _size = n;
    }

    void reserve(size_t n) {
        if (_cap < n) {
            _p = (char*) realloc(_p, n);
            assert(_p);
            _cap = n;
        }
    }

    void swap(fastream& fs) {
        if (&fs != this) {
            char buf[sizeof(fastream)];
            memcpy(buf, this, sizeof(fastream));
            memcpy(this, &fs, sizeof(fastream));
            memcpy(&fs, buf, sizeof(fastream));
        }
    }

    void swap(fastream&& fs) {
        fs.swap(*this);
    }

    fastream& append(char c) {
        this->_Ensure(1);
        _p[_size++] = c;
        return *this;
    }

    fastream& append(const void* p, size_t n) {
        this->_Ensure(n);
        memcpy(_p + _size, p, n);
        _size += n;
        return *this;
    }

    fastream& append(const char* s) {
        return this->append(s, strlen(s));
    }

    fastream& append(const std::string& s) {
        return this->append(s.data(), s.size());
    }

    fastream& append(const fastring& s) {
        return this->append(s.data(), s.size());
    }

    fastream& append(const fastream& s) {
        return this->append(s.data(), s.size());
    }

    fastream& append(char c, size_t n) {
        this->_Ensure(n);
        memset(_p + _size, c, n);
        _size += n;
        return *this;
    }

    fastream& append(size_t n, char c) {
        return this->append(c, n);
    }

    template <typename T>
    fastream& append(const T& v) {
        return this->append(&v, sizeof(T));
    }

    fastream& operator<<(bool v) {
        if (v) return this->append("true", 4);
        return this->append("false", 5);
    }

    fastream& operator<<(char v) {
        return this->append(v);
    }

    fastream& operator<<(unsigned char v) {
        this->_Ensure(4);
        _size += fast::u32toa(v, _p + _size);
        return *this;
    }

    fastream& operator<<(short v) {
        this->_Ensure(8);
        _size += fast::i32toa(v, _p + _size);
        return *this;
    }

    fastream& operator<<(unsigned short v) {
        this->_Ensure(8);
        _size += fast::u32toa(v, _p + _size);
        return *this;
    }

    fastream& operator<<(int v) {
        this->_Ensure(12);
        _size += fast::i32toa(v, _p + _size);
        return *this;
    }

    fastream& operator<<(unsigned int v) {
        this->_Ensure(12);
        _size += fast::u32toa(v, _p + _size);
        return *this;
    }

    fastream& operator<<(long v) {
        this->_Ensure(24);
        _size += fast::i64toa(v, _p + _size);
        return *this;
    }

    fastream& operator<<(unsigned long v) {
        this->_Ensure(24);
        _size += fast::u64toa(v, _p + _size);
        return *this;
    }

    fastream& operator<<(long long v) {
        this->_Ensure(24);
        _size += fast::i64toa(v, _p + _size);
        return *this;
    }

    fastream& operator<<(unsigned long long v) {
        this->_Ensure(24);
        _size += fast::u64toa(v, _p + _size);
        return *this;
    }

    fastream& operator<<(const char* v) {
        return this->append(v, strlen(v));
    }

    fastream& operator<<(const std::string& v) {
        return this->append(v.data(), v.size());
    }

    fastream& operator<<(const fastring& v) {
        return this->append(v.data(), v.size());
    }

    fastream& operator<<(const fastream& fs) {
        return this->append(fs.data(), fs.size());
    }

    fastream& operator<<(const void* v) {
        this->_Ensure(20);
        _size += fast::u64toh((uint64)v, _p + _size);
        return *this;
    }

    fastream& operator<<(float v) {
        this->_Ensure(24);
        _size += fast::dtoa(v, _p + _size);
        return *this;
    }

    fastream& operator<<(double v) {
        this->_Ensure(24);
        _size += fast::dtoa(v, _p + _size);
        return *this;
    }

  private:
    void _Ensure(size_t n) {
        if (_cap < _size + n) this->reserve((_cap * 3 >> 1) + n);
    }

  private:
    size_t _cap;
    size_t _size;
    char* _p;
};

// magicstream is for creating a fastring without memory copy
//   fastring s = (magicstream() << "hello" << 123).str();
class magicstream {
  public:
    explicit magicstream(size_t cap = 16) {
        new (_buf) fastream(cap + 16);
        _fs.resize(16);
    }
    ~magicstream() {}

    template<typename T>
    magicstream& operator<<(const T& t) {
        _fs << t;
        return *this;
    }

    fastream& stream() {
        return _fs;
    }

    fastring str() const {
        unsigned* p = (unsigned*) _fs.data();
        p[0] = (unsigned) _fs.capacity() - 16;
        p[1] = (unsigned) _fs.size() - 16;
        p[2] = 1;
        return *(fastring*)&p;
    }

  private:
    union {
        char _buf[sizeof(fastream)];
        fastream _fs;
    };
};
