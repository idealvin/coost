#pragma once

#include "clist.h"
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
namespace xx {

template<class T>
struct less {
    template<class X, class Y>
    bool operator()(X&& x, Y&& y) const {
        return static_cast<X&&>(x) < static_cast<Y&&>(y);
    }
};

template<>
struct less<const char*> {
    bool operator()(const char* x, const char* y) const {
        return x != y && strcmp(x, y) < 0;
    }
};

template<class T>
struct hash {
    size_t operator()(const T& x) const noexcept {
        return std::hash<T>()(x);
    }
};

template<>
struct hash<const char*> {
    size_t operator()(const char* x) const noexcept {
        return murmur_hash(x, strlen(x));
    }
};

template<class T>
struct eq {
    template<class X, class Y>
    bool operator()(X&& x, Y&& y) const {
        return static_cast<X&&>(x) == static_cast<Y&&>(y);
    }
};

template<>
struct eq<const char*> {
    bool operator()(const char* x, const char* y) const {
        return x == y || strcmp(x, y) == 0;
    }
};

} // xx

template<class T, class Alloc = co::stl_allocator<T>>
using vector = std::vector<T, Alloc>;

template<class T, class Alloc = co::stl_allocator<T>>
using deque = std::deque<T, Alloc>;

template<class T, class Alloc = co::stl_allocator<T>>
using list = std::list<T, Alloc>;

template<
    class K, class V,
    class L = xx::less<K>,
    class Alloc = co::stl_allocator<std::pair<const K, V>>
> using map = std::map<K, V, L, Alloc>;

template<
    class K, class V,
    class L = xx::less<K>,
    class Alloc = co::stl_allocator<std::pair<const K, V>>
> using multimap = std::multimap<K, V, L, Alloc>;

template<
    class K, class L = xx::less<K>,
    class Alloc = co::stl_allocator<K>
> using set = std::set<K, L, Alloc>;

template<
    class K, class L = xx::less<K>,
    class Alloc = co::stl_allocator<K>
> using multiset = std::multiset<K, L, Alloc>;

template<
    class K, class V,
    class Hash = xx::hash<K>,
    class Pred = xx::eq<K>,
    class Alloc = co::stl_allocator<std::pair<const K, V>>
> using hash_map = std::unordered_map<K, V, Hash, Pred, Alloc>;

template<
    class K,
    class Hash = xx::hash<K>,
    class Pred = xx::eq<K>,
    class Alloc = co::stl_allocator<K>
> using hash_set = std::unordered_set<K, Hash, Pred, Alloc>;

template<typename K, typename V>
class lru_map {
  public:
    typedef typename co::hash_map<K, V>::iterator iterator;
    typedef typename co::hash_map<K, V>::key_type key_type;
    typedef typename co::hash_map<K, V>::value_type value_type;

    lru_map() : _capacity(1024) {}
    ~lru_map() = default;

    explicit lru_map(size_t capacity) {
        _capacity = capacity > 0 ? capacity : 1024;
    }

    lru_map(lru_map&& x) {
        _kv.swap(x._kv);
        _ki.swap(x._ki);
        _kl.swap(x._kl);
        _capacity = x._capacity;
    }

    size_t size()    const { return _kv.size(); }
    bool empty()     const { return this->size() == 0; }
    iterator begin() const { return ((lru_map*)this)->_kv.begin(); }
    iterator end()   const { return ((lru_map*)this)->_kv.end(); }

    iterator find(const key_type& key) {
        iterator it = _kv.find(key);
        if (it != _kv.end() && _kl.front() != key) {
            auto ki = _ki.find(key);
            _kl.splice(_kl.begin(), _kl, ki->second); // move key to the front
            ki->second = _kl.begin();
        }
        return it;
    }

    // The key is not inserted if it already exists.
    template<typename Key, typename Val>
    void insert(Key&& key, Val&& value) {
        if (_kv.size() >= _capacity) {
            K k = _kl.back();
            _kl.pop_back();
            _kv.erase(k);
            _ki.erase(k);
        }
        auto r = _kv.emplace(std::forward<Key>(key), std::forward<Val>(value));
        if (r.second) {
            _kl.push_front(key);
            _ki[key] = _kl.begin();
        }
    }

    void erase(iterator it) {
        if (it != _kv.end()) {
            auto ki = _ki.find(it->first);
            _kl.erase(ki->second);
            _ki.erase(ki);
            _kv.erase(it);
        }
    }

    void erase(const key_type& key) {
        this->erase(_kv.find(key));
    }

    void clear() {
        _kv.clear();
        _ki.clear();
        _kl.clear();
    }

    void swap(lru_map& x) noexcept {
        _kv.swap(x._kv);
        _ki.swap(x._ki);
        _kl.swap(x._kl);
        std::swap(_capacity, x._capacity);
    }

    void swap(lru_map&& x) noexcept {
        x.swap(*this);
    }

  private:
    co::hash_map<K, V> _kv;
    co::hash_map<K, typename co::list<K>::iterator> _ki;
    co::list<K> _kl;  // key list
    size_t _capacity; // max capacity
    DISALLOW_COPY_AND_ASSIGN(lru_map);
};

} // co
