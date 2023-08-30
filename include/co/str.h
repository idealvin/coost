#pragma once

#include "error.h"
#include "fastring.h"
#include "stl.h"

namespace str {

// replace substring @sub (len: @m) with @to (len: @l) in string @s (len: @n)
// try @t times at most (0 for unlimited)
__coapi fastring replace(const char* s, size_t n, const char* sub, size_t m, const char* to, size_t l, size_t t=0);

inline fastring replace(const char* s, size_t n, const char* sub, const char* to, size_t t=0) {
    return replace(s, n, sub, strlen(sub), to, strlen(to), t);
}

// str::replace("xooxoox", "oo", "ee");     ->  "xeexeex" 
// str::replace("xooxoox", "oo", "ee", 1);  ->  "xeexoox" 
inline fastring replace(const char* s, const char* sub, const char* to, size_t t=0) {
    return replace(s, strlen(s), sub, to, t);
}

inline fastring replace(const fastring& s, const char* sub, const char* to, size_t t=0) {
    return replace(s.data(), s.size(), sub, to, t);
}

inline fastring replace(const std::string& s, const char* sub, const char* to, size_t t=0) {
    return replace(s.data(), s.size(), sub, to, t);
}

// split string @s (len: @n) by delimiter @c 
// try @t times at most (0 for unlimited)
__coapi co::vector<fastring> split(const char* s, size_t n, char c, size_t t=0);

// split string @s (len: @n) by delimiter @c (len: @m)
// try @t times at most (0 for unlimited)
__coapi co::vector<fastring> split(const char* s, size_t n, const char* c, size_t m, size_t t=0);

// str::split("|x|y|", '|');    ->  [ "", "x", "y" ]
// str::split("xooy", 'o');     ->  [ "x", "", "y" ]
// str::split("xooy", 'o', 1);  ->  [ "x", "oy" ]
inline co::vector<fastring> split(const char* s, char c, size_t t=0) {
    return split(s, strlen(s), c, t);
}

inline co::vector<fastring> split(const fastring& s, char c, size_t t=0) {
    return split(s.data(), s.size(), c, t);
}

inline co::vector<fastring> split(const std::string& s, char c, size_t t=0) {
    return split(s.data(), s.size(), c, t);
}

inline co::vector<fastring> split(const char* s, const char* c, size_t t=0) {
    return split(s, strlen(s), c, strlen(c), t);
}

inline co::vector<fastring> split(const fastring& s, const char* c, size_t t=0) {
    return split(s.data(), s.size(), c, strlen(c), t);
}

inline co::vector<fastring> split(const std::string& s, const char* c, size_t t=0) {
    return split(s.data(), s.size(), c, strlen(c), t);
}

// remove chars in @c from string @s at the left or right side, or both sides.
// @d: 'l' or 'L' for left, 'r' or 'R' for right, 'b' for both.
//   - str::trim(" xx\r\n");            ->  "xx"
//   - str::trim("abxxa", "ab", 'l');   ->  "xxa"
inline fastring trim(const char* s, const char* c=" \t\r\n", char d='b') {
    fastring x(s); x.trim(c, d); return x;
}

inline fastring trim(const char* s, char c, char d = 'b') {
    fastring x(s); x.trim(c, d); return x;
}

inline fastring trim(const fastring& s, const char* c=" \t\r\n", char d='b') {
    fastring x(s); x.trim(c, d); return x;
}

inline fastring trim(const fastring& s, char c, char d='b') {
    fastring x(s); x.trim(c, d); return x;
}

template<typename ...X>
inline fastring strip(X&& ...x) {
    return trim(std::forward<X>(x)...);
}

// convert string to built-in types 
//   - Returns false or 0 if the conversion failed, and the error code will be set to 
//     ERANGE or EINVAL. On success, the error code will be 0.
//   - Call co::error() to get the error number. DO NOT use `errno` directly as it is 
//     not safe on windows.
__coapi bool to_bool(const char* s);
__coapi int32 to_int32(const char* s);
__coapi int64 to_int64(const char* s);
__coapi uint32 to_uint32(const char* s);
__coapi uint64 to_uint64(const char* s);
__coapi double to_double(const char* s);

inline bool to_bool(const fastring& s)        { return to_bool(s.c_str()); }
inline bool to_bool(const std::string& s)     { return to_bool(s.c_str()); }
inline int32 to_int32(const fastring& s)      { return to_int32(s.c_str()); }
inline int32 to_int32(const std::string& s)   { return to_int32(s.c_str()); }
inline int64 to_int64(const fastring& s)      { return to_int64(s.c_str()); }
inline int64 to_int64(const std::string& s)   { return to_int64(s.c_str()); }
inline uint32 to_uint32(const fastring& s)    { return to_uint32(s.c_str()); }
inline uint32 to_uint32(const std::string& s) { return to_uint32(s.c_str()); }
inline uint64 to_uint64(const fastring& s)    { return to_uint64(s.c_str()); }
inline uint64 to_uint64(const std::string& s) { return to_uint64(s.c_str()); }
inline double to_double(const fastring& s)    { return to_double(s.c_str()); }
inline double to_double(const std::string& s) { return to_double(s.c_str()); }


// convert built-in types to string 
template<typename T>
inline fastring from(T t) {
    fastring s(24);
    s << t;
    return s;
}

namespace xx {

inline void cat(fastring&) {}

template<typename X, typename ...V>
inline void cat(fastring& s, X&& x, V&& ... v) {
    (fastream&)s << std::forward<X>(x);
    cat(s, std::forward<V>(v)...);
}

template<typename T>
inline fastring dbg(const T& x) {
    fastring s(32);
    (fastream&)s << x;
    return s;
}
} // xx

inline fastring cat() { return fastring(); }

// concatenate any number of elements into a string
//   - str::cat("hello ", 23);              ==>  "hello 23"
//   - str::cat("127.0.0.1", ':', 7777);    ==>  "127.0.0.1:7777"
template<typename ...X>
inline fastring cat(X&& ... x) {
    fastring s(64);
    xx::cat(s, std::forward<X>(x)...);
    return s;
}

template<typename K, typename V>
inline fastring dbg(const std::pair<K, V>& x) {
    return xx::dbg(x);
}

template<typename T>
inline fastring dbg(const std::vector<T>& x) {
    return xx::dbg(x);
}

template<typename T>
inline fastring dbg(const co::vector<T>& x) {
    return xx::dbg(x);
}

template<typename T>
inline fastring dbg(const std::list<T>& x) {
    return xx::dbg(x);
}

template<typename T>
inline fastring dbg(const co::list<T>& x) {
    return xx::dbg(x);
}

template<typename T>
inline fastring dbg(const std::deque<T>& x) {
    return xx::dbg(x);
}

template<typename T>
inline fastring dbg(const co::deque<T>& x) {
    return xx::dbg(x);
}

template<typename T>
inline fastring dbg(const std::set<T>& x) {
    return xx::dbg(x);
}

template<typename T>
inline fastring dbg(const co::set<T>& x) {
    return xx::dbg(x);
}

template<typename K, typename V>
inline fastring dbg(const std::map<K, V>& x) {
    return xx::dbg(x);
}

template<typename K, typename V>
inline fastring dbg(const co::map<K, V>& x) {
    return xx::dbg(x);
}

template<typename T>
inline fastring dbg(const std::unordered_set<T>& x) {
    return xx::dbg(x);
}

template<typename T>
inline fastring dbg(const co::hash_set<T>& x) {
    return xx::dbg(x);
}

template<typename K, typename V>
inline fastring dbg(const std::unordered_map<K, V>& x) {
    return xx::dbg(x);
}

template<typename K, typename V>
inline fastring dbg(const co::hash_map<K, V>& x) {
    return xx::dbg(x);
}

template<typename K, typename V>
inline fastring dbg(const co::lru_map<K, V>& x) {
    return xx::dbg(x);
}

} // namespace str
