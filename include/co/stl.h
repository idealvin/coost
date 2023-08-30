#pragma once

#include "clist.h"
#include "table.h"
#include "vector.h"
#include "fastream.h"
#include "hash/murmur_hash.h"
#include <list>
#include <deque>
#include <vector>
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

    lru_map(lru_map&& x)
        : _kv(std::move(x._kv)), _ki(std::move(x._ki)), _kl(std::move(x._kl)) {
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
            const auto& k = r.first->first;
            _kl.push_front(k);
            _ki[k] = _kl.begin();
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

namespace xx {

struct Fmt {
    fastream& fmt(fastream& fs, const char* s, size_t n) {
        fs.append('"');
        for (size_t i = 0; i < n; ++i) {
            const char c = s[i];
            switch (c) {
              case '"':
                fs.append("\\\"", 2);
                break;
              case '\r':
                fs.append("\\r", 2);
                break;
              case '\n':
                fs.append("\\n", 2);
                break;
              case '\t':
                fs.append("\\t", 2);
                break;
              case '\b':
                fs.append("\\b", 2);
                break;
              case '\f':
                fs.append("\\f", 2);
                break;
              case '\\':
                fs.append("\\\\", 2);
                break;
              default:
                fs.append(c);
                break;
            }
        }
        return fs.append('"');
    }

    fastream& fmt(fastream& fs, const char* s) {
        return fmt(fs, s, strlen(s));
    }

    fastream& fmt(fastream& fs, const fastring& s) {
        return fmt(fs, s.data(), s.size());
    }

    fastream& fmt(fastream& fs, const std::string& s) {
        return fmt(fs, s.data(), s.size());
    }

    fastream& fmt(fastream& fs, char x) { return fs << x; }
    fastream& fmt(fastream& fs, signed char x) { return fs << x; }
    fastream& fmt(fastream& fs, unsigned char x) { return fs << x; }
    fastream& fmt(fastream& fs, bool x) { return fs << x; }
    fastream& fmt(fastream& fs, float x) { return fs << x; }
    fastream& fmt(fastream& fs, double x) { return fs << x; }
    fastream& fmt(fastream& fs, short x) { return fs << x; }
    fastream& fmt(fastream& fs, int x) { return fs << x; }
    fastream& fmt(fastream& fs, long x) { return fs << x; }
    fastream& fmt(fastream& fs, long long x) { return fs << x; }
    fastream& fmt(fastream& fs, unsigned short x) { return fs << x; }
    fastream& fmt(fastream& fs, unsigned int x) { return fs << x; }
    fastream& fmt(fastream& fs, unsigned long x) { return fs << x; }
    fastream& fmt(fastream& fs, unsigned long long x) { return fs << x; }
    fastream& fmt(fastream& fs, const void* x) { return fs << x; }

    template<typename K, typename V>
    fastream& fmt(fastream& fs, const std::pair<K, V>& x) {
        fmt(fs, x.first);
        fs << ':';
        return fmt(fs, x.second);
    }

    template<typename T>
    fastream& fmt(fastream& fs, const T& beg, const T& end, char c1, char c2) {
        if (beg != end) {
            fs << c1;
            for (T it = beg; it != end; ++it) {
                fmt(fs, *it);
                fs << ',';
            }
            fs.back() = c2;
        } else {
            fs << c1 << c2;
        }
        return fs;
    }

    template<typename T>
    fastream& fmt(fastream& fs, const co::vector<T>& x) {
        return fmt(fs, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    fastream& fmt(fastream& fs, const std::vector<T>& x) {
        return fmt(fs, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    fastream& fmt(fastream& fs, const co::deque<T>& x) {
        return fmt(fs, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    fastream& fmt(fastream& fs, const std::deque<T>& x) {
        return fmt(fs, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    fastream& fmt(fastream& fs, const co::list<T>& x) {
        return fmt(fs, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    fastream& fmt(fastream& fs, const std::list<T>& x) {
        return fmt(fs, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    fastream& fmt(fastream& fs, const co::set<T>& x) {
        return fmt(fs, x.begin(), x.end(), '{', '}');
    }

    template<typename T>
    fastream& fmt(fastream& fs, const std::set<T>& x) {
        return fmt(fs, x.begin(), x.end(), '{', '}');
    }

    template<typename T>
    fastream& fmt(fastream& fs, const co::hash_set<T>& x) {
        return fmt(fs, x.begin(), x.end(), '{', '}');
    }

    template<typename T>
    fastream& fmt(fastream& fs, const std::unordered_set<T>& x) {
        return fmt(fs, x.begin(), x.end(), '{', '}');
    }

    template<typename K, typename V>
    fastream& fmt(fastream& fs, const co::map<K, V>& x) {
        return fmt(fs, x.begin(), x.end(), '{', '}');
    }

    template<typename K, typename V>
    fastream& fmt(fastream& fs, const std::map<K, V>& x) {
        return fmt(fs, x.begin(), x.end(), '{', '}');
    }

    template<typename K, typename V>
    fastream& fmt(fastream& fs, const co::hash_map<K, V>& x) {
        return fmt(fs, x.begin(), x.end(), '{', '}');
    }

    template<typename K, typename V>
    fastream& fmt(fastream& fs, const std::unordered_map<K, V>& x) {
        return fmt(fs, x.begin(), x.end(), '{', '}');
    }

    template<typename K, typename V>
    fastream& fmt(fastream& fs, const co::lru_map<K, V>& x) {
        return fmt(fs, x.begin(), x.end(), '{', '}');
    }
};

} // xx
} // co

template<typename T>
inline fastream& operator<<(fastream& fs, const co::vector<T>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename T>
inline fastream& operator<<(fastream& fs, const std::vector<T>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename T>
inline fastream& operator<<(fastream& fs, const co::deque<T>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename T>
inline fastream& operator<<(fastream& fs, const std::deque<T>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename T>
inline fastream& operator<<(fastream& fs, const co::list<T>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename T>
inline fastream& operator<<(fastream& fs, const std::list<T>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename T>
inline fastream& operator<<(fastream& fs, const co::set<T>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename T>
inline fastream& operator<<(fastream& fs, const std::set<T>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename T>
inline fastream& operator<<(fastream& fs, const co::hash_set<T>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename T>
inline fastream& operator<<(fastream& fs, const std::unordered_set<T>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename K, typename V>
inline fastream& operator<<(fastream& fs, const std::pair<K, V>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename K, typename V>
inline fastream& operator<<(fastream& fs, const co::map<K, V>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename K, typename V>
inline fastream& operator<<(fastream& fs, const std::map<K, V>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename K, typename V>
inline fastream& operator<<(fastream& fs, const co::hash_map<K, V>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename K, typename V>
inline fastream& operator<<(fastream& fs, const std::unordered_map<K, V>& x) {
    return co::xx::Fmt().fmt(fs, x);
}

template<typename K, typename V>
inline fastream& operator<<(fastream& fs, const co::lru_map<K, V>& x) {
    return co::xx::Fmt().fmt(fs, x);
}
