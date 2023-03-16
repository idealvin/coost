#pragma once

#include "fast.h"
#include "fastring.h"

class __coapi fastream : public fast::stream {
  public:
    constexpr fastream() noexcept
        : fast::stream() {
    }
    
    explicit fastream(size_t cap)
        : fast::stream(cap) {
    }

    ~fastream() = default;

    fastream(const fastream&) = delete;
    void operator=(const fastream&) = delete;

    fastream(fastream&& fs) noexcept
        : fast::stream(std::move(fs)) {
    }

    fastream& operator=(fastream&& fs) {
        return (fastream&) fast::stream::operator=(std::move(fs));
    }

    fastring str() const {
        return fastring(_p, _size);
    }

    // It is not safe if p overlaps with the internal buffer of fastream, use 
    // safe_append() in that case.
    fastream& append(const void* p, size_t n) {
        return (fastream&) fast::stream::append(p, n);
    }

    // It is ok if p overlaps with the internal buffer of fastream
    fastream& safe_append(const void* p, size_t n) {
        return (fastream&) fast::stream::safe_append(p, n);
    }

    // append n characters
    fastream& append(size_t n, char c) {
        return (fastream&) fast::stream::append(n, c);
    }

    fastream& append(char c, size_t n) {
        return this->append(n, c);
    }

    fastream& append(const char* s) {
        return this->append(s, strlen(s));
    }

    fastream& safe_append(const char* s) {
        return this->safe_append(s, strlen(s));
    }

    fastream& append(const fastring& s) {
        return this->append(s.data(), s.size());
    }

    fastream& append(const std::string& s) {
        return this->append(s.data(), s.size());
    }

    // It is ok if s is the fastream itself
    fastream& append(const fastream& s) {
        if (&s != this) return this->append(s.data(), s.size());
        this->reserve(_size << 1);
        memcpy(_p + _size, _p, _size); // append itself
        _size <<= 1;
        return *this;
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

    // append binary data of integer types
    fastream& append(uint16 v) {
        return this->append(&v, sizeof(v));
    }

    fastream& append(uint32 v) {
        return this->append(&v, sizeof(v));
    }

    fastream& append(uint64 v) {
        return this->append(&v, sizeof(v));
    }

    fastream& cat() noexcept { return *this; }

    // concatenate fastream to any number of elements
    //   - fastream s("hello");
    //     s.cat(' ', 123);  // s -> "hello 123"
    template<typename X, typename ...V>
    fastream& cat(X&& x, V&& ... v) {
        (*this) << std::forward<X>(x);
        return this->cat(std::forward<V>(v)...);
    }

    fastream& operator<<(bool v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(char v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(signed char v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(unsigned char v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(short v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(unsigned short v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(int v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(unsigned int v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(long v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(unsigned long v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(long long v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(unsigned long long v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(double v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(float v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    // float point number with max decimal places set
    //   - fastream() << dp::_2(3.1415);  // -> 3.14
    fastream& operator<<(const dp::_fpt& v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(const void* v) {
        return (fastream&) fast::stream::operator<<(v);
    }

    fastream& operator<<(std::nullptr_t) {
        return (fastream&) fast::stream::operator<<(nullptr);
    }

    fastream& operator<<(const char* s) {
        return this->append(s, strlen(s));
    }

    fastream& operator<<(const signed char* s) {
        return this->operator<<((const char*)s);
    }

    fastream& operator<<(const unsigned char* s) {
        return this->operator<<((const char*)s);
    }

    fastream& operator<<(const fastring& s) {
        return this->append(s.data(), s.size());
    }

    fastream& operator<<(const std::string& s) {
        return this->append(s.data(), s.size());
    }

    fastream& operator<<(const fastream& s) {
        return this->append(s);
    }
};
