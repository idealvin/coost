#pragma once

#include "fast.h"
#include "fastring.h"

class __codec fastream : public fast::stream {
  public:
    constexpr fastream() noexcept : fast::stream() {}
    
    explicit fastream(size_t cap) : fast::stream(cap) {}

    fastream(void* p, size_t size, size_t cap)
        : fast::stream(p, size, cap) {
    }

    ~fastream() = default;

    fastream(const fastream&) = delete;
    void operator=(const fastream&) = delete;

    fastream(fastream&& fs) noexcept
        : fast::stream(std::move(fs)) {
    }

    fastream& operator=(fastream&& fs) noexcept {
        return (fastream&) fast::stream::operator=(std::move(fs));
    }

    // copy data in fastream as a fastring
    fastring str() const {
        return fastring(_p, _size);
    }

    fastream& append(const void* p, size_t n) {
        return (fastream&) fast::stream::append(p, n);
    }

    fastream& append(const char* s) {
        return this->append(s, strlen(s));
    }

    fastream& append(const fastring& s) {
        return this->append(s.data(), s.size());
    }

    fastream& append(const std::string& s) {
        return this->append(s.data(), s.size());
    }

    fastream& append(const fastream& s) {
        if (&s != this) return this->append(s.data(), s.size());
        this->reserve(_size << 1);
        memcpy(_p + _size, _p, _size); // append itself
        _size <<= 1;
        return *this;
    }

    // append n characters
    fastream& append(size_t n, char c) {
        return (fastream&) fast::stream::append(n, c);
    }

    fastream& append(char c, size_t n) {
        return this->append(n, c);
    }

    fastream& append(char c) {
        return (fastream&) fast::stream::append(c);
    }

    fastream& append(signed char c) {
        return this->append((char)c);
    }

    fastream& append(unsigned char c) {
        return this->append((char)c);
    }

    fastream& append(short v) {
        return this->append(&v, sizeof(v));
    }

    fastream& append(unsigned short v) {
        return this->append(&v, sizeof(v));
    }

    fastream& append(int v) {
        return this->append(&v, sizeof(v));
    }

    fastream& append(unsigned int v) {
        return this->append(&v, sizeof(v));
    }

    fastream& append(long v) {
        return this->append(&v, sizeof(v));
    }

    fastream& append(unsigned long v) {
        return this->append(&v, sizeof(v));
    }

    fastream& append(long long v) {
        return this->append(&v, sizeof(v));
    }

    fastream& append(unsigned long long v) {
        return this->append(&v, sizeof(v));
    }

    fastream& operator<<(const char* s) {
        return this->append(s);
    }

    fastream& operator<<(const signed char* s) {
        return this->operator<<((const char*)s);
    }

    fastream& operator<<(const unsigned char* s) {
        return this->operator<<((const char*)s);
    }

    fastream& operator<<(const std::string& s) {
        return this->append(s);
    }

    fastream& operator<<(const fastring& s) {
        return this->append(s);
    }

    fastream& operator<<(const fastream& s) {
        return this->append(s);
    }

    // for built-in types or pointer types
    template<typename T>
    fastream& operator<<(T v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& cat() { return *this; }

    // concatenate fastream to any number of elements
    //   - fastream s("hello");
    //     s.cat(' ', 123);  // s -> "hello 123"
    template<typename X, typename ...V>
    fastream& cat(X&& x, V&& ... v) {
        this->operator<<(std::forward<X>(x));
        return this->cat(std::forward<V>(v)...);
    }
};
