#pragma once

#include "fastream.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace str {

// split("x y z", ' ');  ->  [ "x", "y", "z" ]
// split("|x|y|", '|');  ->  [ "", "x", "y" ]
// split("xooy", "oo");  ->  [ "x", "y"]
// split("xooy", 'o', 1);  ->  [ "x", "oy" ]
std::vector<fastring> split(const char* s, char c, uint32 maxsplit=0);
std::vector<fastring> split(const fastring& s, char c, uint32 maxsplit=0);
std::vector<fastring> split(const char* s, const char* c, uint32 maxsplit=0);

inline std::vector<fastring> split(const fastring& s, const char* c, uint32 maxsplit=0) {
    return split(s.c_str(), c, maxsplit);
}

// replace("xooxoox", "oo", "ee");     ->  "xeexeex"
// replace("xooxoox", "oo", "ee", 1);  ->  "xeexoox"
fastring replace(const char* s, const char* sub, const char* to, uint32 maxreplace=0);
fastring replace(const fastring& s, const char* sub, const char* to, uint32 maxreplace=0);

// strip("abxxa", "ab")       ->  "xx"     strip both left and right.
// strip("abxxa", "ab", 'l')  ->  "xxa"    strip left only.
// strip("abxxa", "ab", 'r')  ->  "abxx"   strip right only.
fastring strip(const char* s, const char* c=" \t\r\n", char d='b');
fastring strip(const char* s, char c, char d = 'b');
fastring strip(const fastring& s, const char* c=" \t\r\n", char d='b');
fastring strip(const fastring& s, char c, char d='b');
fastring strip(const fastring& s, const fastring& c, char d='b');

// convert string to built-in types, throw fastring on any error
bool to_bool(const char* s);
int32 to_int32(const char* s);
int64 to_int64(const char* s);
uint32 to_uint32(const char* s);
uint64 to_uint64(const char* s);
double to_double(const char* s);

template<typename S>
inline bool to_bool(const S& s) {
    return to_bool(s.c_str());
}

template<typename S>
inline int32 to_int32(const S& s) {
    return to_int32(s.c_str());
}

template<typename S>
inline int64 to_int64(const S& s) {
    return to_int64(s.c_str());
}

template<typename S>
inline uint32 to_uint32(const S& s) {
    return to_uint32(s.c_str());
}

template<typename S>
inline uint64 to_uint64(const S& s) {
    return to_uint64(s.c_str());
}

template<typename S>
inline double to_double(const S& s) {
    return to_double(s.c_str());
}

// convert built-in types to string
inline fastring from(const fastring& s) {
    return s;
}

template<typename T>
inline fastring from(const T& t) {
    return (magicstream(24) << t).str();
}

namespace xx {
inline void dbg(const char* s, fastream& fs) {
    fs << '\"' << s << '\"';
}

inline void dbg(const std::string& s, fastream& fs) {
    fs << '\"' << s << '\"';
}

inline void dbg(const fastring& s, fastream& fs) {
    fs << '\"' << s << '\"';
}

template<typename T>
inline void dbg(const T& t, fastream& fs) {
    fs << t;
}

template<typename K, typename V>
inline void dbg(const std::pair<K, V>& x, fastream& fs) {
    dbg(x.first, fs);
    fs << ':';
    dbg(x.second, fs);
}

template<typename T>
void dbg(const T& beg, const T& end, char c1, char c2, fastream& fs) {
    if (beg == end) {
        fs << c1 << c2;
        return;
    }

    fs << c1;
    for (T it = beg; it != end; ++it) {
        dbg(*it, fs);
        fs << ',';
    }
    fs.back() = c2;
}
} // xx

template<typename K, typename V>
inline fastring dbg(const std::pair<K, V>& x) {
    magicstream ms(32);
    xx::dbg(x, ms.stream());
    return ms.str();;
}

template<typename T>
inline fastring dbg(const T& beg, const T& end, char c1, char c2) {
    magicstream ms(128);
    xx::dbg(beg, end, c1, c2, ms.stream());
    return ms.str();
}

template<typename T>
inline fastring dbg(const std::vector<T>& v) {
    return dbg(v.begin(), v.end(), '[', ']');
}

template<typename T>
inline fastring dbg(const std::set<T>& v) {
    return dbg(v.begin(), v.end(), '{', '}');
}

template<typename K, typename V>
inline fastring dbg(const std::map<K, V>& v) {
    return dbg(v.begin(), v.end(), '{', '}');
}

template<typename T>
inline fastring dbg(const std::unordered_set<T>& v) {
    return dbg(v.begin(), v.end(), '{', '}');
}

template<typename K, typename V>
inline fastring dbg(const std::unordered_map<K, V>& v) {
    return dbg(v.begin(), v.end(), '{', '}');
}

} // namespace str
