#pragma once

#include "fast.h"
#include "fastring.h"

class fastream : public fast::bastream {
  public:
    constexpr fastream() noexcept : fast::bastream() {}
    
    explicit fastream(size_t cap) : fast::bastream(cap) {}

    ~fastream() = default;

    fastream(const fastream&) = delete;
    void operator=(const fastream&) = delete;

    fastream(fastream&& fs) noexcept
        : fast::bastream(std::move(fs)) {
    }

    fastream& operator=(fastream&& fs) noexcept {
        return (fastream&) fast::bastream::operator=(std::move(fs));
    }

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
        return (fastream&) fast::bastream::append(s);
    }

    fastream& append(char c, size_t n) {
        return (fastream&) fast::bastream::append(c, n);
    }

    fastream& append(size_t n, char c) {
        return this->append(c, n);
    }

    template<typename T>
    fastream& append(T v) {
        return (fastream&) fast::bastream::append(v);
    }

    fastream& operator<<(const char* s) {
        return (fastream&) fast::bastream::operator<<(s);
    }

    fastream& operator<<(const std::string& s) {
        return (fastream&) fast::bastream::operator<<(s);
    }

    fastream& operator<<(const fastring& s) {
        return (fastream&) this->_Append(s.data(), s.size());
    }

    fastream& operator<<(const fastream& s) {
        if (&s != this) return (fastream&) this->_Append(s.data(), s.size());
        if (!s.empty()) {
            this->reserve(_size << 1);
            memcpy(_p + _size, _p, _size);
            _size <<= 1;
        }
        return *this;
    }

    template<typename T>
    fastream& operator<<(T v) {
        return (fastream&) fast::bastream::operator<<(v);
    }
};
