#pragma once

#include "god.h"
#include "fast.h"
#include "fastring.h"

class __coapi fastream : public fast::stream {
  public:
    fastream() noexcept
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

    // append a single character
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

    fastream& cat() { return *this; }

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

    template<typename T>
    using _is_supported_type = god::enable_if_t<
        god::is_basic<T>() || god::is_pointer<T>() ||
        god::is_literal_string<T>() || god::is_c_str<T>() ||
        god::is_same<god::remove_cv_t<T>, fastring, fastream, std::string>(), int
    >;

    // Special optimization for string literal like "hello". The length of a string 
    // literal can be get at compile-time, no need to call strlen().
    template<typename T, _is_supported_type<god::remove_ref_t<T>> = 0>
    fastream& operator<<(T&& t) {
        using X = god::remove_ref_t<T>; // remove & or &&
        using C = god::remove_cv_t<X>;  // remove const, volatile

        constexpr int N =
            god::is_literal_string<X>() ? 1 :
            god::is_c_str<X>() ? 2 :
            god::is_same<C, fastring, fastream, std::string>() ? 3 :
            0;

        return this->_out(std::forward<T>(t), I<N>());
    }

  private:
    template<int N> struct I {};

    // built-in types or pointer types
    template<typename T>
    fastream& _out(T&& t, I<0>) {
        return (fastream&) fast::stream::operator<<(std::forward<T>(t));
    }

    // string literal like "hello"
    template<typename T>
    fastream& _out(T&& t, I<1>) {
        return this->append(t, sizeof(t) - 1);
    }

    // const char* or char*
    template<typename T>
    fastream& _out(T&& t, I<2>) {
        return this->append(t);
    }

    // fastring, fastream, std::string
    template<typename T>
    fastream& _out(T&& t, I<3>) {
        return this->append((god::const_ref_t<T>)t);
    }
};
