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

    // It is not safe if p points to area of the fastream itself, this is by design.
    // Convert fastream to fastring instead in that case:
    //   fastream s; ((fastring&)s).append(s.c_str() + 1);
    fastream& append(const void* p, size_t n) {
        return (fastream&) fast::stream::append(p, n);
    }

    // append n characters
    fastream& append(size_t n, char c) {
        return (fastream&) fast::stream::append(n, c);
    }

    fastream& append(char c, size_t n) {
        return this->append(n, c);
    }

    template<typename S, god::enable_if_t<god::is_literal_string<god::remove_ref_t<S>>(), int> = 0>
    fastream& append(S&& s) {
        return this->append(s, sizeof(s) - 1);
    }

    template<typename S, god::enable_if_t<god::is_c_str<god::remove_ref_t<S>>(), int> = 0>
    fastream& append(S&& s) {
        return this->append(s, strlen(s));
    }

    template<typename S, god::enable_if_t<
        god::has_method_c_str<S>() && !god::is_same<god::remove_cvref_t<S>, fastream>(), int> = 0
    >
    fastream& append(S&& s) {
        return this->append(s.data(), s.size());
    }

    template<typename S, god::enable_if_t<
        god::is_same<god::remove_cvref_t<S>, fastream>(), int> = 0
    >
    fastream& append(S&& s) {
        if (&s != this) return this->append(s.data(), s.size());
        this->reserve(_size << 1);
        memcpy(_p + _size, _p, _size); // append itself
        _size <<= 1;
        return *this;
    }

    template<typename S, god::enable_if_t<
        god::is_same<god::remove_cvref_t<S>, co::stref>(), int> = 0
    >
    fastream& append(S&& s) {
        return this->append(s.data(), s.size());
    }

    // append a single character
    template<typename T, god::enable_if_t<
        god::is_same<god::remove_cvref_t<T>, char, signed char, unsigned char>(), int> = 0
    >
    fastream& append(T&& c) {
        return (fastream&) fast::stream::append((char)c);
    }

    // append binary data of integer types
    template<typename T, god::enable_if_t<god::is_integer<god::remove_cvref_t<T>>(), int> = 0>
    fastream& append(T&& v) {
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

    // set max decimal places as mdp.n
    fast::stream::fpstream operator<<(co::maxdp mdp) {
        return fast::stream::operator<<(mdp);
    }

    fastream& operator<<(const signed char* s) {
        return (fastream&) fast::stream::append(s, strlen((const char*)s));
    }

    fastream& operator<<(const unsigned char* s) {
        return (fastream&) fast::stream::append(s, strlen((const char*)s));
    }

    template<typename S, god::enable_if_t<god::is_literal_string<god::remove_ref_t<S>>(), int> = 0>
    fastream& operator<<(S&& s) {
        return this->append(s, sizeof(s) - 1);
    }

    template<typename S, god::enable_if_t<god::is_c_str<god::remove_ref_t<S>>(), int> = 0>
    fastream& operator<<(S&& s) {
        return this->append(s, strlen(s));
    }

    template<typename S, god::enable_if_t<god::has_method_c_str<god::remove_cvref_t<S>>(), int> = 0>
    fastream& operator<<(S&& s) {
        return this->append(std::forward<S>(s));
    }

    template<typename S, god::enable_if_t<
        god::is_same<god::remove_cvref_t<S>, co::stref>(), int> = 0
    >
    fastream& operator<<(S&& s) {
        return this->append(s.data(), s.size());
    }

    template<typename T, god::enable_if_t<god::is_basic<god::remove_ref_t<T>>(), int> = 0>
    fastream& operator<<(T&& t) {
        return (fastream&) fast::stream::operator<<(std::forward<T>(t));
    }

    template<typename T, god::enable_if_t<
        !god::is_c_str<god::remove_ref_t<T>>() && god::is_pointer<god::remove_ref_t<T>>(), int> = 0
    >
    fastream& operator<<(T&& t) {
        return (fastream&) fast::stream::operator<<(std::forward<T>(t));
    }
};
