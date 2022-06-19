#pragma once

#include "array.h"
#include "table.h"
#include "hash/murmur_hash.h"
#include <vector>
#include <list>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace co {

template <class T, class Alloc = co::stl_allocator<T>>
using vector = std::vector<T, Alloc>;

template <class T, class Alloc = co::stl_allocator<T>>
using deque = std::deque<T, Alloc>;

template <class T, class Alloc = co::stl_allocator<T>>
using list = std::list<T, Alloc>;

template <class T>
struct _Less {
    template <class X, class Y>
    bool operator()(X&& x, Y&& y) const {
        return static_cast<X&&>(x) < static_cast<Y&&>(y);
    }
};

template <>
struct _Less<const char*> {
    bool operator()(const char* x, const char* y) const {
        return strcmp(x, y) < 0;
    }
};

template <
    class K, class V,
    class L = _Less<K>,
    class Alloc = co::stl_allocator<std::pair<const K, V>>
> using map = std::map<K, V, L, Alloc>;

template <
    class K, class V,
    class L = _Less<K>,
    class Alloc = co::stl_allocator<std::pair<const K, V>>
> using multimap = std::multimap<K, V, L, Alloc>;

template <
    class K, class L = _Less<K>,
    class Alloc = co::stl_allocator<K>
> using set = std::set<K, L, Alloc>;

template <
    class K, class L = _Less<K>,
    class Alloc = co::stl_allocator<K>
> using multiset = std::multiset<K, L, Alloc>;

template <class T>
struct _Hash {
    size_t operator()(const T& x) const noexcept {
        return std::hash<T>()(x);
    }
};

// take const char* as a string
template <>
struct _Hash<const char*> {
    size_t operator()(const char* x) const noexcept {
        return murmur_hash(x, strlen(x));
    }
};

template <class T>
struct _Eq {
    template <class X, class Y>
    bool operator()(X&& x, Y&& y) const {
        return static_cast<X&&>(x) == static_cast<Y&&>(y);
    }
};

// compare c-style string with strcmp
template <>
struct _Eq<const char*> {
    bool operator()(const char* x, const char* y) const {
        return x == y || strcmp(x, y) == 0;
    }
};

template <
    class K, class V,
    class Hash = _Hash<K>,
    class Pred = _Eq<K>,
    class Alloc = co::stl_allocator<std::pair<const K, V>>
> using hash_map = std::unordered_map<K, V, Hash, Pred, Alloc>;

template <
    class K,
    class Hash = _Hash<K>,
    class Pred = _Eq<K>,
    class Alloc = co::stl_allocator<K>
> using hash_set = std::unordered_set<K, Hash, Pred, Alloc>;

} // co
