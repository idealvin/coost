#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4706) // if ((a = x))
#endif

#include "fast.h"
#include "hash/murmur_hash.h"
#include <string>
#include <ostream>

namespace str {
__coapi char* memrchr(const char* s, char c, size_t n);
__coapi char* memmem(const char* s, size_t n, const char* p, size_t m);
__coapi char* memimem(const char* s, size_t n, const char* p, size_t m);
__coapi char* memrmem(const char* s, size_t n, const char* p, size_t m);
__coapi bool match(const char* s, size_t n, const char* p, size_t m);

inline int memcmp(const char* s, size_t n, const char* p, size_t m) {
    const int i = ::memcmp(s, p, n < m ? n : m);
    return i != 0 ? i : (n < m ? -1 : n != m);
}
} // str

class __coapi fastring : public fast::stream {
  public:
    static const size_t npos = (size_t)-1;

    constexpr fastring() noexcept
        : fast::stream() {
    }

    explicit fastring(size_t cap)
        : fast::stream(cap) {
    }

    ~fastring() = default;

    /* `fastring(n, c)` collides with `fastring(const void*, size_t)`:
       both take two arguments, and the Crust C++ subset resolves
       overloads by argument count. A constructor cannot be renamed, so
       the repeat form becomes a static factory -- available to both
       builds -- and the constructor itself is C++-only. The buffer form
       keeps the constructor because it is the more fundamental of the
       two and has far more callers. */
    static fastring repeat(size_t n, char c) {
        fastring s(n + 1);
        s.resize(n);
        memset(s.data(), c, n);
        return s;
    }

#ifndef CO_CRUST
    fastring(size_t n, char c)
        : fast::stream(n + 1, n) {
        memset(_p, c, n);
    }
#endif /* CO_CRUST */

    fastring(char* p, size_t cap, size_t size)
        : fast::stream(p, cap, size) {
    }

    fastring(const void* s, size_t n)
        : fast::stream(n + !!n, n) {
        memcpy(_p, s, n);
    }

    /* The one-argument constructors collide the same way the
       two-argument ones did. `fastring(size_t cap)` keeps the
       constructor -- it is what the library itself uses, thirty times,
       to pre-size a buffer -- and the string forms become static
       factories available to both builds. */
    static fastring from_cstr(const char* s) {
        fastring r(s, strlen(s));
        return r;
    }

    static fastring from_stdstr(const std::string& s) {
        fastring r(s.data(), s.size());
        return r;
    }

#ifndef CO_CRUST
    fastring(const char* s)
        : fastring(s, strlen(s)) {
    }

    fastring(const std::string& s)
        : fastring(s.data(), s.size()) {
    }
#endif /* CO_CRUST */

    /* Initialises the base directly rather than delegating to
       `fastring(const void*, size_t)`: constructor delegation is not in
       the Crust C++ subset. This is that constructor's body. */
    fastring(const fastring& s)
        : fast::stream(s.size() + !!s.size(), s.size()) {
        memcpy(_p, s.data(), s.size());
    }

    fastring(fastring&& s) noexcept
        : fast::stream(std::move(s)) {
    }

    fastring& operator=(fastring&& s) {
        /* Member-wise rather than delegating to the base's move
           assignment with a base-scoped operator call:
           a base-scoped operator call is not in the Crust C++ subset. The
           steal below is what the base's move assignment does, on fields
           the base declares protected for exactly this reach. */
        if (&s != this) {
            if (_p) co::free(_p, _cap);
            _cap = s._cap; _size = s._size; _p = s._p;
            s._p = 0;
            s._cap = s._size = 0;
        }
        return *this;
    }

    fastring& operator=(const fastring& s) {
        return &s != this ? *this->assign_raw(s.data(), s.size()) : *this;
    }

#ifndef CO_CRUST
    /* Converting assignments: same arity as the copy assignment above,
       so both would lower to one `fastring__assign` symbol. Absent under
       the subset; `assign_mem(s, n)` is the named form callers use. */
    fastring& operator=(const std::string& s) {
        return *this->assign_raw(s.data(), s.size());
    }

    fastring& operator=(const char* s) {
        return *this->assign_mem(s, strlen(s));
    }
#endif /* CO_CRUST */

    fastring* assign_mem(const void* s, size_t n) {
        if (!this->_inside((const char*)s)) return this->assign_raw(s, n);
        assert((const char*)s + n <= _p + _size);
        if (s != _p) memmove(_p, s, n);
        _size = n;
        return this;
    }

    fastring* assign_n(size_t n, char c) {
        this->reserve(n + 1);
        memset(_p, c, n);
        _size = n;
        return this;
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    fastring& assign(const void* s, size_t n) { return *this->assign_mem(s, n); }
    fastring& assign(size_t n, char c) { return *this->assign_n(n, c); }
#endif /* CO_CRUST */


#ifndef CO_CRUST
    /* Forwards to the converting assignments above, which are C++-only. */
    template<typename S>
    fastring& assign(S&& s) {
        return this->operator=(std::forward<S>(s));
    }
#endif /* CO_CRUST */

    fastring* append(const void* p, size_t n) {
        return (fastring*) fast::stream::append(p, n);
    }
 
    // like append(), but will not check if p overlaps with the internal memory
    fastring* append_nomchk(const void* p, size_t n) {
        return (fastring*) fast::stream::append_nomchk(p, n);
    }

    fastring* append_cstr(const char* s) {
        return this->append(s, strlen(s));
    }

    // like append(), but will not check if s overlaps with the internal memory
    fastring* append_nomchk(const char* s) {
        return this->append_nomchk(s, strlen(s));
    }

    fastring* append_str(const fastring& s) {
        if (&s != this) return this->append_nomchk(s.data(), s.size());
        this->reserve((_size << 1) + !!_size);
        memcpy(_p + _size, _p, _size); // append itself
        _size <<= 1;
        return this;
    }

    fastring* append_stdstr(const std::string& s) {
        return this->append_nomchk(s.data(), s.size());
    }

    fastring* append_chars(size_t n, char c) {
        return (fastring*) fast::stream::append_chars(n, c);
    }

    fastring* append_char(char c) {
        return (fastring*) fast::stream::append(c);
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    fastring& append(const char* s) { return *this->append_cstr(s); }
    fastring& append(const fastring& s) { return *this->append_str(s); }
    fastring& append(const std::string& s) { return *this->append_stdstr(s); }
    fastring& append(char c) { return *this->append_char(c); }
#endif /* CO_CRUST */


    fastring* push_back(char c) { return this->append_char(c); }

    char pop_back() { return _p[--_size]; }

    /* All four `operator+=` overloads take one argument, so they all
       lower to one `fastring__augadd` symbol. The fastring operand keeps
       the operator; the rest are C++-only and route to the named appends,
       which exist in both builds. */
    fastring& operator+=(const fastring& s) {
        return *this->append_str(s);
    }

#ifndef CO_CRUST
    fastring& operator+=(const std::string& s) {
        return *this->append_stdstr(s);
    }

    fastring& operator+=(const char* s) {
        return *this->append_cstr(s);
    }

    fastring& operator+=(char c) {
        return *this->append_char(c);
    }
#endif /* CO_CRUST */

    fastring* cat() { return this; }

/* The stream-insertion API. `operator<<` is permanently out of the
   Crust C++ subset, and these are also same-arity overloads, which the
   subset resolves by argument count -- so under `-D CO_CRUST` the whole
   family is absent and callers use the named `append*` methods on
   `fast::stream` instead. The ordinary C++ build is unchanged. */
#ifndef CO_CRUST
    template<typename X, typename ...V>
    fastring& cat(X&& x, V&& ... v) {
        (*this) << std::forward<X>(x);
        return *this->cat(std::forward<V>(v)...);
    }

    fastring& operator<<(bool v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(char v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(signed char v) {
        return this->operator<<((char)v);
    }

    fastring& operator<<(unsigned char v) {
        return this->operator<<((char)v);
    }

    fastring& operator<<(short v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(unsigned short v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(int v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(unsigned int v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(long v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(unsigned long v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(long long v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(unsigned long long v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(double v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(float v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    // float point number with max decimal places set
    //   - fastring() << dp::_2(3.1415);  // -> 3.14
    fastring& operator<<(const dp::_fpt& v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(const void* v) {
        return (fastring&) fast::stream::operator<<(v);
    }

    fastring& operator<<(std::nullptr_t) {
        return (fastring&) fast::stream::operator<<(nullptr);
    }

    fastring& operator<<(const char* s) {
        return *this->append(s, strlen(s));
    }

    fastring& operator<<(const signed char* s) {
        return this->operator<<((const char*)s);
    }

    fastring& operator<<(const unsigned char* s) {
        return this->operator<<((const char*)s);
    }

    fastring& operator<<(const fastring& s) {
        return this->append(s);
    }

    fastring& operator<<(const std::string& s) {
        return *this->append_nomchk(s.data(), s.size());
    }
#endif /* CO_CRUST */

    /* The two- and four-argument forms are unique, so they keep the bare
       name and carry the work. The rest are split by the type of their
       *string* argument -- which sits first in the whole-string forms and
       third in the substring ones, so this family is named by hand rather
       than by the first parameter. `_sub` marks a comparison of a
       substring of this string; `_sub_sub` marks one of both. */
    int compare(const char* s, size_t n) const {
        return str::memcmp(_p, _size, s, n);
    }

    int compare(size_t pos, size_t len, const char* s, size_t n) const {
        const intptr_t x = (intptr_t)(_size - pos);
        if (x > 0) return str::memcmp(_p + pos, len < (size_t)x ? len : x, s, n);
        return str::memcmp(_p, 0, s, n);
    }

    int compare_cstr(const char* s) const {
        return this->compare(s, strlen(s));
    }

    int compare_str(const fastring& s) const noexcept {
        return this->compare(s.data(), s.size());
    }

    int compare_stdstr(const std::string& s) const noexcept {
        return this->compare(s.data(), s.size());
    }

    int compare_sub_cstr(size_t pos, size_t len, const char* s) const {
        return this->compare(pos, len, s, strlen(s));
    }

    int compare_sub_str(size_t pos, size_t len, const fastring& s) const {
        return this->compare(pos, len, s.data(), s.size());
    }

    int compare_sub_stdstr(size_t pos, size_t len, const std::string& s) const {
        return this->compare(pos, len, s.data(), s.size());
    }

    int compare_sub_str_sub(size_t pos, size_t len, const fastring& s,
                            size_t spos, size_t n=npos) const {
        const intptr_t x = (intptr_t)(s.size() - spos);
        if (x > 0) return this->compare(pos, len, s.data() + spos, n < (size_t)x ? n : x);
        return this->compare(pos, len, s.data(), 0);
    }

    int compare_sub_stdstr_sub(size_t pos, size_t len, const std::string& s,
                               size_t spos, size_t n=npos) const {
        const intptr_t x = (intptr_t)(s.size() - spos);
        if (x > 0) return this->compare(pos, len, s.data() + spos, n < (size_t)x ? n : x);
        return this->compare(pos, len, s.data(), 0);
    }

#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent under the Crust C++
       subset, which resolves overloads by argument count. */
    int compare(const char* s) const { return this->compare_cstr(s); }
    int compare(const fastring& s) const noexcept { return this->compare_str(s); }
    int compare(const std::string& s) const noexcept { return this->compare_stdstr(s); }
    int compare(size_t pos, size_t len, const char* s) const {
        return this->compare_sub_cstr(pos, len, s);
    }
    int compare(size_t pos, size_t len, const fastring& s) const {
        return this->compare_sub_str(pos, len, s);
    }
    int compare(size_t pos, size_t len, const std::string& s) const {
        return this->compare_sub_stdstr(pos, len, s);
    }
    int compare(size_t pos, size_t len, const fastring& s, size_t spos,
                size_t n=npos) const {
        return this->compare_sub_str_sub(pos, len, s, spos, n);
    }
    int compare(size_t pos, size_t len, const std::string& s, size_t spos,
                size_t n=npos) const {
        return this->compare_sub_stdstr_sub(pos, len, s, spos, n);
    }
#endif /* CO_CRUST */

    bool contains_char(char c) const {
        return this->find(c) != npos;
    }

    bool contains_cstr(const char* s) const {
        return this->find(s) != npos;
    }

    bool contains_str(const fastring& s) const {
        return this->contains_cstr(s.c_str());
    }

    bool contains_stdstr(const std::string& s) const {
        return this->contains_cstr(s.c_str());
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    bool contains(char c) const { return this->contains_char(c); }
    bool contains(const char* s) const { return this->contains_cstr(s); }
    bool contains(const fastring& s) const { return this->contains_str(s); }
    bool contains(const std::string& s) const { return this->contains_stdstr(s); }
#endif /* CO_CRUST */


    bool starts_with_char(char c) const {
        return !this->empty() && this->front() == c;
    }

    bool starts_with(const char* s, size_t n) const {
        return n == 0 || (n <= _size && ::memcmp(_p, s, n) == 0);
    }

    bool starts_with_cstr(const char* s) const {
        return this->starts_with(s, strlen(s));
    }

    bool starts_with_str(const fastring& s) const {
        return this->starts_with(s.data(), s.size());
    }

    bool starts_with_stdstr(const std::string& s) const {
        return this->starts_with(s.data(), s.size());
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    bool starts_with(char c) const { return this->starts_with_char(c); }
    bool starts_with(const char* s) const { return this->starts_with_cstr(s); }
    bool starts_with(const fastring& s) const { return this->starts_with_str(s); }
    bool starts_with(const std::string& s) const { return this->starts_with_stdstr(s); }
#endif /* CO_CRUST */


    bool ends_with_char(char c) const {
        return !this->empty() && this->back() == c;
    }

    bool ends_with(const char* s, size_t n) const {
        return n == 0 || (n <= _size && ::memcmp(_p + _size - n, s, n) == 0);
    }

    bool ends_with_cstr(const char* s) const {
        return this->ends_with(s, strlen(s));
    }

    bool ends_with_str(const fastring& s) const {
        return this->ends_with(s.data(), s.size());
    }

    bool ends_with_stdstr(const std::string& s) const {
        return this->ends_with(s.data(), s.size());
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    bool ends_with(char c) const { return this->ends_with_char(c); }
    bool ends_with(const char* s) const { return this->ends_with_cstr(s); }
    bool ends_with(const fastring& s) const { return this->ends_with_str(s); }
    bool ends_with(const std::string& s) const { return this->ends_with_stdstr(s); }
#endif /* CO_CRUST */


    fastring* remove_prefix(const char* s, size_t n) {
        return this->starts_with(s, n) ? this->trim_n(n, 'l') : this;
    }

    fastring* remove_prefix_cstr(const char* s) {
        return this->remove_prefix(s, strlen(s));
    }

    fastring* remove_prefix_str(const fastring& s) {
        return this->remove_prefix(s.data(), s.size());
    }

    fastring* remove_prefix_stdstr(const std::string& s) {
        return this->remove_prefix(s.data(), s.size());
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    fastring& remove_prefix(const char* s) { return *this->remove_prefix_cstr(s); }
    fastring& remove_prefix(const fastring& s) { return *this->remove_prefix_str(s); }
    fastring& remove_prefix(const std::string& s) { return *this->remove_prefix_stdstr(s); }
#endif /* CO_CRUST */


    fastring* remove_suffix(const char* s, size_t n) {
        if (this->ends_with(s, n)) this->resize(this->size() - n); 
        return this;
    }

    fastring* remove_suffix_cstr(const char* s) {
        return this->remove_suffix(s, strlen(s));
    }

    fastring* remove_suffix_str(const fastring& s) {
        return this->remove_suffix(s.data(), s.size());
    }

    fastring* remove_suffix_stdstr(const std::string& s) {
        return this->remove_suffix(s.data(), s.size());
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    fastring& remove_suffix(const char* s) { return *this->remove_suffix_cstr(s); }
    fastring& remove_suffix(const fastring& s) { return *this->remove_suffix_str(s); }
    fastring& remove_suffix(const std::string& s) { return *this->remove_suffix_stdstr(s); }
#endif /* CO_CRUST */


    // remove character @c at the left or right side, or both sides
    // @d: 'l' or 'L' for left, 'r' or 'R' for right, otherwise for both sides
    fastring* trim_char(char c, char d='b');

    fastring* trim_u8(unsigned char c, char d='b') {
        return this->trim_char((char)c, d);
    }

    fastring* trim_i8(signed char c, char d='b') {
        return this->trim_char((char)c, d);
    }

    // remove characters in @s at the left or right side, or both sides
    // @d: 'l' or 'L' for left, 'r' or 'R' for right, otherwise for both sides
    fastring* trim_cstr(const char* s=" \t\r\n", char d='b');
    
    // remove the first n characters or the last n characters, or both
    // @d: 'l' or 'L' for left, 'r' or 'R' for right, otherwise for both sides
    fastring* trim_n(size_t n, char d='b');

    fastring* trim_i32(int n, char d='b') {
        return this->trim_n((size_t)n, d);
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    fastring& trim(char c, char d='b') { return *this->trim_char(c, d); }
    fastring& trim(unsigned char c, char d='b') { return *this->trim_u8(c, d); }
    fastring& trim(signed char c, char d='b') { return *this->trim_i8(c, d); }
    fastring& trim(const char* s=" \t\r\n", char d='b') { return *this->trim_cstr(s, d); }
    fastring& trim(size_t n, char d='b') { return *this->trim_n(n, d); }
    fastring& trim(int n, char d='b') { return *this->trim_i32(n, d); }
#endif /* CO_CRUST */


    // the same as trim
    template<typename ...X>
    fastring* strip(X&& ...x) {
        return &this->trim(std::forward<X>(x)...);
    }

    // replace substring @sub (len: @n) with @to (len: @m)
    // try @t times at most
    fastring* replace(const char* sub, size_t n, const char* to, size_t m, size_t t=0);

    fastring* replace_cstr(const char* sub, const char* to, size_t t=0) {
        return this->replace(sub, strlen(sub), to, strlen(to), t);
    }

    fastring* replace_str(const fastring& sub, const fastring& to, size_t t=0) {
        return this->replace(sub.data(), sub.size(), to.data(), to.size(), t);
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    fastring& replace(const char* sub, const char* to, size_t t=0) { return *this->replace_cstr(sub, to, t); }
    fastring& replace(const fastring& sub, const fastring& to, size_t t=0) { return *this->replace_str(sub, to, t); }
#endif /* CO_CRUST */


    fastring* tolower();
    fastring* toupper();

    fastring lower() const {
        fastring s(*this); s.tolower(); return s;
    }

    fastring upper() const {
        fastring s(*this); s.toupper(); return s;
    }

    /* A returned local is moved out; a constructed temporary in the
       return expression has nothing to move from, so the caller would
       get a copy of something already released. Named locals instead. */
    fastring substr(size_t pos) const {
        if (pos < _size) {
            fastring s(_p + pos, _size - pos);
            return s;
        }
        fastring e;
        return e;
    }

    fastring substr(size_t pos, size_t len) const {
        if (pos < _size) {
            const size_t n = _size - pos;
            fastring s(_p + pos, len < n ? len : n);
            return s;
        }
        fastring e;
        return e;
    }

    // find char @c
    size_t find_char(char c) const {
        if (!this->empty()) {
            char* const p = (char*) memchr(_p, c, _size);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find char @c from @pos
    size_t find_char_from(char c, size_t pos) const {
        if (pos < _size) {
            char* const p = (char*) memchr(_p + pos, c, _size - pos);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find char @c in [pos, pos + len)
    size_t find_char_in(char c, size_t pos, size_t len) const {
        if (pos < _size) {
            const size_t n = _size - pos;
            char* const p = (char*) memchr(_p + pos, c, len < n ? len : n);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find sub string @s
    size_t find_cstr(const char* s) const {
        char* const p = str::memmem(_p, _size, s, strlen(s));
        return p ? p - _p : npos;
    }

    // find @s (length: @n) from @pos
    size_t find_cstr_in(const char* s, size_t pos, size_t n) const {
        if (pos < _size) {
            char* const p = str::memmem(_p + pos, _size - pos, s, n);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find @s from @pos
    size_t find_cstr_from(const char* s, size_t pos) const {
        return this->find_cstr_in(s, pos, strlen(s));
    }

    // find @s from @pos
    size_t find_str(const fastring& s, size_t pos=0) const {
        return this->find_cstr_in(s.data(), pos, s.size());
    }

    // find @s from @pos
    size_t find_stdstr(const std::string& s, size_t pos=0) const {
        return this->find_cstr_in(s.data(), pos, s.size());
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    size_t find(char c) const { return this->find_char(c); }
    size_t find(char c, size_t pos) const { return this->find_char_from(c, pos); }
    size_t find(char c, size_t pos, size_t len) const { return this->find_char_in(c, pos, len); }
    size_t find(const char* s) const { return this->find_cstr(s); }
    size_t find(const char* s, size_t pos) const { return this->find_cstr_from(s, pos); }
    size_t find(const char* s, size_t pos, size_t n) const { return this->find_cstr_in(s, pos, n); }
    size_t find(const std::string& s, size_t pos=0) const { return this->find_stdstr(s, pos); }
    size_t find(const fastring& s, size_t pos=0) const { return this->find_str(s, pos); }
#endif /* CO_CRUST */


    // find @s (ignore the case)
    size_t ifind(const char* s) const {
        char* const p = str::memimem(_p, _size, s, strlen(s));
        return p ? p - _p : npos;
    }

    // find @s (length: @n) from @pos (ignore the case)
    size_t ifind(const char* s, size_t pos, size_t n) const {
        if (pos < _size) {
            char* const p = str::memimem(_p + pos, _size - pos, s, n);
            return p ? p - _p : npos;
        }
        return npos;
    }

    // find @s from @pos (ignore the case)
    size_t ifind_cstr(const char* s, size_t pos) const {
        return this->ifind(s, pos, strlen(s));
    }

    // find @s from @pos (ignore the case)
    size_t ifind_str(const fastring& s, size_t pos=0) const {
        return this->ifind(s.data(), pos, s.size());
    }

    // find @s from @pos (ignore the case)
    size_t ifind_stdstr(const std::string& s, size_t pos=0) const {
        return this->ifind(s.data(), pos, s.size());
    }

    // find char @c from @pos (ignore the case)
    size_t ifind_char(char c, size_t pos=0) const {
        return this->ifind(&c, pos, 1);
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    size_t ifind(char c, size_t pos=0) const { return this->ifind_char(c, pos); }
    size_t ifind(const char* s, size_t pos) const { return this->ifind_cstr(s, pos); }
    size_t ifind(const std::string& s, size_t pos=0) const { return this->ifind_stdstr(s, pos); }
    size_t ifind(const fastring& s, size_t pos=0) const { return this->ifind_str(s, pos); }
#endif /* CO_CRUST */


    // reverse find char @c
    size_t rfind_char(char c) const {
        char* const p = str::memrchr(_p, c, _size);
        return p ? p - _p : npos;
    }

    // reverse find char @c from @pos
    size_t rfind_char_from(char c, size_t pos) const {
        char* const p = str::memrchr(_p, c, pos < _size ? pos + 1 : _size);
        return p ? p - _p : npos;
    }

    // reverse find sub string @s
    size_t rfind_cstr(const char* s) const {
        const size_t n = strlen(s);
        if (n > 0) {
            char* const p = str::memrmem(_p, _size, s, n);
            return p ? p - _p : npos;
        }
        return _size;
    }

    // reverse find @s (length: @n) from @pos
    size_t rfind(const char* s, size_t pos, size_t n) const {
        if (n > 0) {
            char* const p = str::memrmem(_p, pos >= _size ? _size : pos + 1, s, n);
            return p ? p - _p : npos;
        }
        return pos >= _size ? _size : pos;
    }

    // reverse find @s from @pos
    size_t rfind_cstr_from(const char* s, size_t pos) const {
        return this->rfind(s, pos, strlen(s));
    }

    // reverse find @s from @pos
    size_t rfind_str(const fastring& s, size_t pos=npos) const {
        return this->rfind(s.data(), pos, s.size());
    }

    // reverse find @s from @pos
    size_t rfind_stdstr(const std::string& s, size_t pos=npos) const {
        return this->rfind(s.data(), pos, s.size());
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    size_t rfind(char c) const { return this->rfind_char(c); }
    size_t rfind(char c, size_t pos) const { return this->rfind_char_from(c, pos); }
    size_t rfind(const char* s) const { return this->rfind_cstr(s); }
    size_t rfind(const char* s, size_t pos) const { return this->rfind_cstr_from(s, pos); }
    size_t rfind(const std::string& s, size_t pos=npos) const { return this->rfind_stdstr(s, pos); }
    size_t rfind(const fastring& s, size_t pos=npos) const { return this->rfind_str(s, pos); }
#endif /* CO_CRUST */


    // find first char in @s (length: @n) from @pos
    size_t find_first_of(const char* s, size_t pos, size_t n) const;

    // find first char in @s from @pos
    size_t find_first_of_cstr(const char* s, size_t pos=0) const {
        return this->find_first_of(s, pos, strlen(s));
    }

    // find first char in @s from @pos
    size_t find_first_of_str(const fastring& s, size_t pos=0) const {
        return this->find_first_of(s.data(), pos, s.size());
    }

    // find first char in @s from @pos
    size_t find_first_of_stdstr(const std::string& s, size_t pos=0) const {
        return this->find_first_of(s.data(), pos, s.size());
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    size_t find_first_of(const char* s, size_t pos=0) const { return this->find_first_of_cstr(s, pos); }
    size_t find_first_of(const std::string& s, size_t pos=0) const { return this->find_first_of_stdstr(s, pos); }
    size_t find_first_of(const fastring& s, size_t pos=0) const { return this->find_first_of_str(s, pos); }
#endif /* CO_CRUST */


    // find first char not in @s (length: @n) from @pos
    size_t find_first_not_of(const char* s, size_t pos, size_t n) const;

    // find first char not in @s from @pos
    size_t find_first_not_of_cstr(const char* s, size_t pos=0) const {
        return this->find_first_not_of(s, pos, strlen(s));
    }

    // find first char not in @s from @pos
    size_t find_first_not_of_str(const fastring& s, size_t pos=0) const {
        return this->find_first_not_of(s.data(), pos, s.size());
    }

    // find first char not in @s from @pos
    size_t find_first_not_of_stdstr(const std::string& s, size_t pos=0) const {
        return this->find_first_not_of(s.data(), pos, s.size());
    }

    // find first char not equal to @c
    size_t find_first_not_of_char(char c, size_t pos=0) const;
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    size_t find_first_not_of(char c, size_t pos=0) const { return this->find_first_not_of_char(c, pos); }
    size_t find_first_not_of(const char* s, size_t pos=0) const { return this->find_first_not_of_cstr(s, pos); }
    size_t find_first_not_of(const std::string& s, size_t pos=0) const { return this->find_first_not_of_stdstr(s, pos); }
    size_t find_first_not_of(const fastring& s, size_t pos=0) const { return this->find_first_not_of_str(s, pos); }
#endif /* CO_CRUST */


    // find last char in @s (length: @n) from @pos
    size_t find_last_of(const char* s, size_t pos, size_t n) const;

    // find last char in @s from @pos
    size_t find_last_of_cstr(const char* s, size_t pos=npos) const {
        return this->find_last_of(s, pos, strlen(s));
    }

    // find last char in @s from @pos
    size_t find_last_of_str(const fastring& s, size_t pos=npos) const {
        return this->find_last_of(s.data(), pos, s.size());
    }

    // find last char in @s from @pos
    size_t find_last_of_stdstr(const std::string& s, size_t pos=npos) const {
        return this->find_last_of(s.data(), pos, s.size());
    }
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    size_t find_last_of(const char* s, size_t pos=npos) const { return this->find_last_of_cstr(s, pos); }
    size_t find_last_of(const std::string& s, size_t pos=npos) const { return this->find_last_of_stdstr(s, pos); }
    size_t find_last_of(const fastring& s, size_t pos=npos) const { return this->find_last_of_str(s, pos); }
#endif /* CO_CRUST */


    // find last char not in @s (length: @n) from @pos
    size_t find_last_not_of(const char* s, size_t pos, size_t n) const;

    // find last char not in @s from @pos
    size_t find_last_not_of_cstr(const char* s, size_t pos=npos) const {
        return this->find_last_not_of(s, pos, strlen(s));
    }

    // find last char not in @s from @pos
    size_t find_last_not_of_str(const fastring& s, size_t pos=npos) const {
        return this->find_last_not_of(s.data(), pos, s.size());
    }

    // find last char not in @s from @pos
    size_t find_last_not_of_stdstr(const std::string& s, size_t pos=npos) const {
        return this->find_last_not_of(s.data(), pos, s.size());
    }

    // find last char not equal to @c
    size_t find_last_not_of_char(char c, size_t pos=npos) const;
#ifndef CO_CRUST
    /* Type-overloaded spellings: same arity, so absent
       under the Crust C++ subset, which resolves overloads
       by argument count. Each delegates to its named form. */
    size_t find_last_not_of(char c, size_t pos=npos) const { return this->find_last_not_of_char(c, pos); }
    size_t find_last_not_of(const char* s, size_t pos=npos) const { return this->find_last_not_of_cstr(s, pos); }
    size_t find_last_not_of(const std::string& s, size_t pos=npos) const { return this->find_last_not_of_stdstr(s, pos); }
    size_t find_last_not_of(const fastring& s, size_t pos=npos) const { return this->find_last_not_of_str(s, pos); }
#endif /* CO_CRUST */


    // * matches 0 or more characters
    // ? matches exactly one character
    bool match(const char* pattern) const {
        return str::match(_p, _size, pattern, strlen(pattern));
    }

    void shrink() {
        /* A named local rather than a temporary: a reference parameter
           is lowered to a pointer, so the argument needs an address and
           a constructed temporary has none. */
        if (_size + 1 < _cap) {
            fastring s(*this);
            this->swap(s);
        }
    }

  private:
    /* Named `assign_raw`, not `_assign`: a method whose name starts
       with an underscore mangles to `fastring__assign`, which is the
       symbol `operator=` already lowers to. The two collided in the
       emitted C. */
    fastring* assign_raw(const void* s, size_t n) {
        _size = n;
        if (n > 0) {
            this->reserve(n + 1);
            memcpy(_p, s, n);
        }
        return this;
    }

    bool _inside(const char* p) const {
        return _p <= p && p < _p + _size;
    }
};

inline fastring operator+(const fastring& a, char b) {
    fastring s(a.size() + 2);
    s.append_str(a)->append_char(b);
    return s;
}

inline fastring operator+(char a, const fastring& b) {
    fastring s(b.size() + 2);
    s.append_char(a)->append_str(b);
    return s;
}

inline fastring operator+(const fastring& a, const fastring& b) {
    fastring s(a.size() + b.size() + 1);
    s.append_str(a)->append_str(b);
    return s;
}

inline fastring operator+(const fastring& a, const std::string& b) {
    fastring s(a.size() + b.size() + 1);
    s.append_str(a)->append_stdstr(b);
    return s;
}

inline fastring operator+(const std::string& a, const fastring& b) {
    fastring s(a.size() + b.size() + 1);
    s.append_stdstr(a)->append_str(b);
    return s;
}

inline fastring operator+(const fastring& a, const char* b) {
    const size_t n = strlen(b);
    fastring s(a.size() + n + 1);
    s.append_str(a)->append(b, n);
    return s;
}

inline fastring operator+(const char* a, const fastring& b) {
    const size_t n = strlen(a);
    fastring s(b.size() + n + 1);
    s.append(a, n)->append_str(b);
    return s;
}

inline bool operator==(const fastring& a, const fastring& b) {
    return a.compare_str(b) == 0;
}

inline bool operator==(const fastring& a, const std::string& b) {
    return a.compare_stdstr(b) == 0;
}

inline bool operator==(const std::string& a, const fastring& b) {
    return b == a;
}

inline bool operator==(const fastring& a, const char* b) {
    return a.compare_cstr(b) == 0;
}

inline bool operator==(const char* a, const fastring& b) {
    return b == a;
}

inline bool operator!=(const fastring& a, const fastring& b) {
    return !(a == b);
}

inline bool operator!=(const fastring& a, const std::string& b) {
    return !(a == b);
}

inline bool operator!=(const std::string& a, const fastring& b) {
    return !(a == b);
}

inline bool operator!=(const fastring& a, const char* b) {
    return !(a == b);
}

inline bool operator!=(const char* a, const fastring& b) {
    return b != a;
}

inline bool operator<(const fastring& a, const fastring& b) {
    return a.compare_str(b) < 0;
}

inline bool operator<(const fastring& a, const std::string& b) {
    return a.compare_stdstr(b) < 0;
}

inline bool operator<(const std::string& a, const fastring& b) {
    return b.compare_stdstr(a) > 0;
}

inline bool operator<(const fastring& a, const char* b) {
    return a.compare_cstr(b) < 0;
}

inline bool operator>(const fastring& a, const fastring& b) {
    return a.compare_str(b) > 0;
}

inline bool operator>(const fastring& a, const std::string& b) {
    return a.compare_stdstr(b) > 0;
}

inline bool operator>(const std::string& a, const fastring& b) {
    return b.compare_stdstr(a) < 0;
}

inline bool operator>(const fastring& a, const char* b) {
    return a.compare_cstr(b) > 0;
}

inline bool operator<(const char* a, const fastring& b) {
    return b > a;
}

inline bool operator>(const char* a, const fastring& b) {
    return b < a;
}

inline bool operator<=(const fastring& a, const fastring& b) {
    return !(a > b);
}

inline bool operator<=(const fastring& a, const std::string& b) {
    return !(a > b);
}

inline bool operator<=(const std::string& a, const fastring& b) {
    return !(a > b);
}

inline bool operator<=(const fastring& a, const char* b) {
    return !(a > b);
}

inline bool operator<=(const char* a, const fastring& b) {
    return !(b < a);
}

inline bool operator>=(const fastring& a, const fastring& b) {
    return !(a < b);
}

inline bool operator>=(const fastring& a, const std::string& b) {
    return !(a < b);
}

inline bool operator>=(const std::string& a, const fastring& b) {
    return !(a < b);
}

inline bool operator>=(const fastring& a, const char* b) {
    return !(a < b);
}

inline bool operator>=(const char* a, const fastring& b) {
    return !(b > a);
}

#ifndef CO_CRUST
inline std::ostream& operator<<(std::ostream& os, const fastring& s) {
    return os.write(s.data(), s.size());
}
#endif /* CO_CRUST */

namespace std {
template<>
struct hash<fastring> {
    size_t operator()(const fastring& s) const {
        return murmur_hash(s.data(), s.size());
    }
};
} // std

/* A string view whose whole point is three one-argument converting
   constructors -- overload-by-type, which the Crust C++ subset resolves
   by argument *count* and refuses. Its only consumers are cout.h's
   color helpers, themselves absent under -D CO_CRUST, so the class goes
   with them rather than being redesigned. */
#ifndef CO_CRUST
class anystr {
  public:
    constexpr anystr() noexcept : _s(""), _n(0) {}
    constexpr anystr(const char* s, size_t n) noexcept : _s(s), _n(n) {}

    // modern compilers may do strlen for string literals at compile time,
    // see https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
    anystr(const char* s) noexcept : _s(s), _n(strlen(s)) {}

    anystr(const std::string& s) noexcept : _s(s.data()), _n(s.size()) {}
    anystr(const fastring& s) noexcept : _s(s.data()), _n(s.size()) {}

    constexpr const char* data() const noexcept { return _s; }
    constexpr size_t size() const noexcept { return _n; }

  private:
    const char* const _s;
    const size_t _n;
};
#endif /* CO_CRUST */
