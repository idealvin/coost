#pragma once

#include "fast.h"
#include "fastring.h"

class fastream : public fast::stream {
  public:
    constexpr fastream() noexcept : fast::stream() {}
    
    explicit fastream(size_t cap) : fast::stream(cap) {}

    ~fastream() = default;

    fastream(const fastream&) = delete;
    void operator=(const fastream&) = delete;

    fastream(fastream&& fs) noexcept
        : fast::stream(std::move(fs)) {
    }

    fastream& operator=(fastream&& fs) noexcept {
        return (fastream&) fast::stream::operator=(std::move(fs));
    }

    // make a copy of fastring
    fastring str() const {
        return fastring(_p, _size);
    }

    fastream& append(const void* p, size_t n) {
        return (fastream&) this->_Append(p, n);
    }

    fastream& append(const char* s) {
        return this->append(s, strlen(s));
    }

    fastream& append(const fastring& s) {
        return this->append(s.data(), s.size());
    }

    fastream& append(const std::string& s) {
        return (fastream&) fast::stream::append(s);
    }

    fastream& append(size_t n, char c) {
        return (fastream&) fast::stream::append(n, c);
    }

    fastream& append(char c, size_t n) {
        return this->append(n, c);
    }

    fastream& append(const fastream& s) {
        if (&s != this) return this->append(s.data(), s.size());
        this->reserve(_size << 1);
        memcpy(_p + _size, _p, _size); // append itself
        _size <<= 1;
        return *this;
    }

    template<typename T>
    fastream& append(T v) {
        return (fastream&) fast::stream::append(v);
    }

    fastream& operator<<(const char* s) {
        return (fastream&) fast::stream::operator<<(s);
    }

    fastream& operator<<(const std::string& s) {
        return (fastream&) fast::stream::operator<<(s);
    }

    fastream& operator<<(const fastring& s) {
        return this->append(s.data(), s.size());
    }

    fastream& operator<<(const fastream& s) {
        return this->append(s);
    }

    template<typename T>
    fastream& operator<<(T v) {
        return (fastream&) fast::stream::operator<<(v);
    }
};
