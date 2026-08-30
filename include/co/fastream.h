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
        /* Member-wise for the same reason as fastring's: a base-scoped
           operator call is not in the Crust C++ subset. */
        if (&fs != this) {
            if (_p) co::free(_p, _cap);
            _cap = fs._cap; _size = fs._size; _p = fs._p;
            fs._p = 0;
            fs._cap = fs._size = 0;
        }
        return *this;
    }

    fastring str() const {
        return fastring(_p, _size);
    }

    fastream* append(const void* p, size_t n) {
        return (fastream*) fast::stream::append(p, n);
    }

    // like append(), but will not check if p overlaps with the internal memory
    fastream* append_nomchk(const void* p, size_t n) {
        return (fastream*) fast::stream::append_nomchk(p, n);
    }

    /* Named appends. One method per element type, because the Crust C++
       subset resolves overloads by argument *count* -- ten one-argument
       `append`s collapse to one symbol there. These carry the bodies and
       are available to both builds; the type-overloaded spellings below
       are C++-only and delegate here, so the ordinary API is unchanged.
       They return `fastream*` for the same subset reason (a reference
       return is not in it), which is why the delegations dereference. */
    fastream* append_cstr(const char* s) {
        return this->append(s, strlen(s));
    }

    fastream* append_nomchk_cstr(const char* s) {
        return this->append_nomchk(s, strlen(s));
    }

    fastream* append_str(const fastring& s) {
        return this->append_nomchk(s.data(), s.size());
    }

    fastream* append_stdstr(const std::string& s) {
        return this->append_nomchk(s.data(), s.size());
    }

    // appending the fastream itself is ok
    fastream* append_stream(const fastream& s) {
        if (&s != this) return this->append_nomchk(s.data(), s.size());
        this->reserve((_size << 1) + !!_size);
        memcpy(_p + _size, _p, _size); // append itself
        _size <<= 1;
        return this;
    }

    fastream* append_char(char c) {
        return (fastream*) fast::stream::append(c);
    }

    fastream* append_i8(signed char c) { return this->append_char((char)c); }
    fastream* append_u8(unsigned char c) { return this->append_char((char)c); }

    // binary data, 2 / 4 / 8 bytes
    fastream* append_u16(uint16 v) { return this->append_nomchk(&v, sizeof(v)); }
    fastream* append_u32(uint32 v) { return this->append_nomchk(&v, sizeof(v)); }
    fastream* append_u64(uint64 v) { return this->append_nomchk(&v, sizeof(v)); }

    // append n characters
    fastream* append_chars(size_t n, char c) {
        return (fastream*) fast::stream::append_chars(n, c);
    }

#ifndef CO_CRUST
    /* The type-overloaded spellings: same arity, so absent under the
       subset. Each delegates to its named counterpart above. */
    fastream& append(const char* s) { return *this->append_cstr(s); }
    fastream& append_nomchk(const char* s) {
        return *this->append_nomchk_cstr(s);
    }
    fastream& append(const fastring& s) { return *this->append_str(s); }
    fastream& append(const std::string& s) { return *this->append_stdstr(s); }
    fastream& append(const fastream& s) { return *this->append_stream(s); }
    fastream& append(char c) { return *this->append_char(c); }
    fastream& append(signed char c) { return *this->append_i8(c); }
    fastream& append(unsigned char c) { return *this->append_u8(c); }
    fastream& append(uint16 v) { return *this->append_u16(v); }
    fastream& append(uint32 v) { return *this->append_u32(v); }
    fastream& append(uint64 v) { return *this->append_u64(v); }
#endif /* CO_CRUST */

    fastream& cat() noexcept { return *this; }

    // concatenate fastream to any number of elements
    //   - fastream s("hello");
    //     s.cat(' ', 123);  // s -> "hello 123"
/* The stream-insertion API. `operator<<` is permanently out of the
   Crust C++ subset, and these are also same-arity overloads, which the
   subset resolves by argument count -- so under `-D CO_CRUST` the whole
   family is absent and callers use the named `append*` methods on
   `fast::stream` instead. The ordinary C++ build is unchanged. */
#ifndef CO_CRUST
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
        return this->operator<<((char)v);
    }

    fastream& operator<<(unsigned char v) {
        return this->operator<<((char)v);
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
        return *this->append(s, strlen(s));
    }

    fastream& operator<<(const signed char* s) {
        return this->operator<<((const char*)s);
    }

    fastream& operator<<(const unsigned char* s) {
        return this->operator<<((const char*)s);
    }

    fastream& operator<<(const fastring& s) {
        return *this->append_nomchk(s.data(), s.size());
    }

    fastream& operator<<(const std::string& s) {
        return *this->append_nomchk(s.data(), s.size());
    }

    fastream& operator<<(const fastream& s) {
        return this->append(s);
    }
#endif /* CO_CRUST */
};
