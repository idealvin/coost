#pragma once

#include "string.h"

// This library is ported from golang's path package.
// Assume the path separator is '/'.

namespace path {

// Return the shortest path name equivalent to the path.
//   - path::clean("");           ->  "."
//   - path::clean(".//x/");      ->  "x"
//   - path::clean("./x/../..");  ->  ".."
//   - path::clean("/x/../..");   ->  "/"
//   - path::clean("x//y//z");    ->  "x/y/z"
co::string clean(const char* s, size_t n);

inline co::string clean(const char* s) {
    return clean(s, strlen(s));
}

inline co::string clean(const co::string& s) {
    return clean(s.data(), s.size());
}

namespace xx {
inline void join(co::string&) {}

template<typename V, typename ...X>
inline void join(co::string& s, V&& v, X&&... x) {
    const size_t n = s.size();
    s << std::forward<V>(v);
    if (s.size() != n) s << '/';
    join(s, std::forward<X>(x)...);
}
} // namespace xx

// Join any number of path elements into a single path. The result is cleaned.
// All empty elements are ignored.
//   - path::json("", "");      ->  ""
//   - path::json("/x", "y");   ->  "/x/y"
//   - path::json("/x/", "y");  ->  "/x/y"
template<typename ...X>
inline co::string join(X&&... x) {
    co::string s(64);
    xx::join(s, std::forward<X>(x)...);
    return !s.empty() ? clean(s) : s;
}

// Split path by the final slash, separating it into a dir and file name.
// If there is no slash in path, return an empty dir and file set to path.
// The returned values have the property that path = dir+file.
//   - path::split("/a/");   ->  <"/a/", "">
//   - path::split("/a/b");  ->  <"/a/", "b">
std::pair<co::string, co::string> split(const char* s, size_t n);

inline std::pair<co::string, co::string> split(const char* s) {
    return split(s, strlen(s));
}

inline std::pair<co::string, co::string> split(const co::string& s) {
    return split(s.data(), s.size());
};

// Return the dir part of the path. The result is cleaned.
// If the path is empty, return ".".
//   - path::dir("");     -> "."
//   - path::dir("a");    -> "."
//   - path::dir("/a");   -> "/"
//   - path::dir("/a/");  -> "/a"
co::string dir(const char* s, size_t n);

inline co::string dir(const char* s) {
    return dir(s, strlen(s));
}

inline co::string dir(const co::string& s) {
    return dir(s.data(), s.size());
}

// Return the last element of the path. Trailing slashes are removed before 
// extracting the last element.
//
// If the path is empty, return ".".
// If the path consists entirely of slashes, return "/".
//   - path::base("");       ->  "."
//   - path::base("/a/b");   ->  "b"
//   - path::base("/a/b/");  ->  "b"
co::string base(const char* s, size_t n);

inline co::string base(const char* s) {
    return base(s, strlen(s));
}

inline co::string base(const co::string& s) {
    return base(s.data(), s.size());
}

// return file name extension of the path
//   - path::ext("x/x.c")  ->  ".c"
//   - path::ext("a/b")    ->  ""
//   - path::ext("/b.c/")  ->  ""
//   - path::ext("a.")     ->  "."
co::string ext(const char* s, size_t n);

inline co::string ext(const char* s) {
    return ext(s, strlen(s));
}

inline co::string ext(const co::string& s) {
    return ext(s.data(), s.size());
}

} // namespace path
