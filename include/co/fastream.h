#pragma once

#include "god.h"
#include "fast.h"
#include "stref.h"
#include "fastring.h"

class __coapi fastream : public fast::stream {
  public:
    constexpr fastream() noexcept
        : fast::stream() {
    }
    
    explicit fastream(size_t cap)
        : fast::stream(cap) {
    }

    fastream(void* p, size_t size, size_t cap) noexcept
        : fast::stream(p, size, cap) {
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

    // copy data in fastream as a fastring
    fastring str() const {
        return fastring(_p, _size);
    }

    // It is not safe if p points to part of the fastream itself, this is by design.
    // Use safe_append() in that case.
    fastream& append(const void* p, size_t n) {
        return (fastream&) fast::stream::append(p, n);
    }

    // p may points to part of the fastream itself.
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

    fastream& append(const fastream& s) {
        if (&s != this) return this->append(s.data(), s.size());
        this->reserve(_size << 1);
        memcpy(_p + _size, _p, _size); // append itself
        _size <<= 1;
        return *this;
    }

    fastream& append(const co::stref& s) {
        return this->append(s.data(), s.size());
    }

    // append a single character
    fastream& append(char c) {
        return (fastream&)fast::stream::append(c);
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
        this->operator<<(std::forward<X>(x));
        return this->cat(std::forward<V>(v)...);
    }

    friend class fpstream;
    class fpstream {
      public:
        fpstream(fastream* s, int mdp) noexcept : s(s), mdp(mdp) {}

        fpstream& operator<<(double v) {
            s->ensure(mdp + 8);
            s->_size += fast::dtoa(v, s->_p + s->_size, mdp);
            return *this;
        }

        fpstream& operator<<(float v) {
            return this->operator<<((double)v);
        }

        fpstream& operator<<(maxdp_t x) noexcept {
            mdp = x.n;
            return *this;
        }

        template <typename T>
        fpstream& operator<<(T&& t) {
            s->operator<<(std::forward<T>(t));
            return *this;
        }

        fastream* s;
        int mdp;
    };

    // set max decimal places for float point number, mdp must > 0
    fpstream maxdp(int mdp) noexcept {
        return fpstream(this, mdp);
    }

    // set max decimal places as mdp.n
    fpstream operator<<(maxdp_t mdp) {
        return fpstream(this, mdp.n);
    }

    fastream& operator<<(const signed char* s) {
        return this->append(s, strlen((const char*)s));
    }

    fastream& operator<<(const unsigned char* s) {
        return this->append(s, strlen((const char*)s));
    }

    template<typename S, god::enable_if_t<god::is_literal_string<god::remove_ref_t<S>>(), int> = 0>
    fastream& operator<<(S&& s) {
        return this->append(s, sizeof(s) - 1);
    }

    template<typename S, god::enable_if_t<god::is_c_str<god::remove_ref_t<S>>(), int> = 0>
    fastream& operator<<(S&& s) {
        return this->append(s, strlen(s));
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

    fastream& operator<<(const co::stref& s) {
        return this->append(s.data(), s.size());
    }

    template<typename T, god::enable_if_t<god::is_basic<god::remove_ref_t<T>>(), int> = 0>
    fastream& operator<<(T&& t) {
        return (fastream&) fast::stream::operator<<(std::forward<T>(t));
    }

    template<typename T, god::enable_if_t<
        god::is_pointer<god::remove_ref_t<T>>() && !god::is_c_str<god::remove_ref_t<T>>(), int> = 0
    >
    fastream& operator<<(T&& p) {
        return (fastream&) fast::stream::operator<<(std::forward<T>(p));
    }
};
