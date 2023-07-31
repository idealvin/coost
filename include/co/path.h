#pragma once

#include "fastring.h"

// This library is ported from golang's path package.
// Assume the path separator is '/'.

namespace path {

// Return the shortest path name equivalent to the path.
//   - path::clean("");           ->  "."
//   - path::clean(".//x/");      ->  "x"
//   - path::clean("./x/../..");  ->  ".."
//   - path::clean("/x/../..");   ->  "/"
//   - path::clean("x//y//z");    ->  "x/y/z"
__coapi fastring clean(const char* s, size_t n);

inline fastring clean(const char* s) {
    return clean(s, strlen(s));
}

inline fastring clean(const fastring& s) {
    return clean(s.data(), s.size());
}

namespace xx {
inline void join(fastring&) {}

template<typename S, typename ...X>
inline void join(fastring& f, S&& s, X&&... x) {
    const size_t n = f.size();
    f << std::forward<S>(s);
    if (f.size() != n) f << '/';
    join(f, std::forward<X>(x)...);
}
} // namespace xx

// Join any number of path elements into a single path. The result is cleaned. 
// All empty elements are ignored.
//   - path::json("", "");      ->  ""
//   - path::json("/x", "y");   ->  "/x/y"
//   - path::json("/x/", "y");  ->  "/x/y"
template<typename ...S>
inline fastring join(S&&... s) {
    fastring v(64);
    xx::join(v, std::forward<S>(s)...);
    return !v.empty() ? clean(v) : v;
}

// Split path by the final slash, separating it into a dir and file name.
// If there is no slash in path, return an empty dir and file set to path.
// The returned values have the property that path = dir+file.
//   - path::split("/a/");   ->  <"/a/", "">
//   - path::split("/a/b");  ->  <"/a/", "b">
__coapi std::pair<fastring, fastring> split(const char* s, size_t n);

inline std::pair<fastring, fastring> split(const char* s) {
    return split(s, strlen(s));
}

inline std::pair<fastring, fastring> split(const fastring& s) {
    return split(s.data(), s.size());
};

// Return the dir part of the path. The result is cleaned.
// If the path is empty, return ".".
//   - path::dir("");     -> "."
//   - path::dir("a");    -> "."
//   - path::dir("/a");   -> "/"
//   - path::dir("/a/");  -> "/a"
__coapi fastring dir(const char* s, size_t n);

inline fastring dir(const char* s) {
    return dir(s, strlen(s));
}

inline fastring dir(const fastring& s) {
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
__coapi fastring base(const char* s, size_t n);

inline fastring base(const char* s) {
    return base(s, strlen(s));
}

inline fastring base(const fastring& s) {
    return base(s.data(), s.size());
}

// return file name extension of the path
//   - path::ext("x/x.c")  ->  ".c"
//   - path::ext("a/b")    ->  ""
//   - path::ext("/b.c/")  ->  ""
//   - path::ext("a.")     ->  "."
__coapi fastring ext(const char* s, size_t n);

inline fastring ext(const char* s) {
    return ext(s, strlen(s));
}

inline fastring ext(const fastring& s) {
    return ext(s.data(), s.size());
}

} // namespace path
