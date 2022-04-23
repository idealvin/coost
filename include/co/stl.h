#pragma once

#include "alloc.h"
#include "table.h"
#include "vector.h"
#include <map>
#include <set>
#include <list>
#include <deque>
#include <unordered_map>
#include <unordered_set>

namespace co {

template <
    class K, class V,
    class L = std::less<K>,
    class Alloc = co::stl_allocator<std::pair<const K, V>>
> using map = std::map<K, V, L, Alloc>;

template <
    class K, class V,
    class L = std::less<K>,
    class Alloc = co::stl_allocator<std::pair<const K, V>>
> using multimap = std::multimap<K, V, L, Alloc>;

template <
    class K, class L = std::less<K>,
    class Alloc = co::stl_allocator<K>
> using set = std::set<K, L, Alloc>;

template <
    class K, class L = std::less<K>,
    class Alloc = co::stl_allocator<K>
> using multiset = std::multiset<K, L, Alloc>;

template <class T, class Alloc = co::stl_allocator<T>>
using deque = std::deque<T, Alloc>;

template <class T, class Alloc = co::stl_allocator<T>>
using list = std::list<T, Alloc>;

template <
    class K, class V,
    class Hash = std::hash<K>,
    class Pred = std::equal_to<K>,
    class Alloc = co::stl_allocator<std::pair<const K, V>>
> using hash_map = std::unordered_map<K, V, Hash, Pred, Alloc>;

template <
    class K,
    class Hash = std::hash<K>,
    class Pred = std::equal_to<K>,
    class Alloc = co::stl_allocator<K>
> using hash_set = std::unordered_set<K, Hash, Pred, Alloc>;

} // co
