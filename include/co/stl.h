#pragma once

#include "string.h"
#include <list>
#include <deque>
#include <queue>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace co {

template<class T>
struct less {
    template<class X, class Y>
    bool operator()(X&& x, Y&& y) const noexcept {
        return static_cast<X&&>(x) < static_cast<Y&&>(y);
    }
};

template<>
struct less<const char*> {
    bool operator()(const char* x, const char* y) const noexcept {
        return x != y && strcmp(x, y) < 0;
    }
};

template<class T>
struct greater {
    template<class X, class Y>
    bool operator()(X&& x, Y&& y) const noexcept {
        return static_cast<X&&>(x) > static_cast<Y&&>(y);
    }
};

template<>
struct greater<const char*> {
    bool operator()(const char* x, const char* y) const noexcept {
        return x != y && strcmp(x, y) > 0;
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
        return co::murmur_hash(x, strlen(x));
    }
};

namespace xx {

template<class T>
struct eq {
    template<class X, class Y>
    bool operator()(X&& x, Y&& y) const noexcept {
        return static_cast<X&&>(x) == static_cast<Y&&>(y);
    }
};

template<>
struct eq<const char*> {
    bool operator()(const char* x, const char* y) const noexcept {
        return x == y || strcmp(x, y) == 0;
    }
};

} // xx

//template<class T, class Alloc = co::stl_allocator<T>>
//using vector = std::vector<T, Alloc>;

template<class T, class Alloc = co::stl_allocator<T>>
using deque = std::deque<T, Alloc>;

template<class T, class Compare = less<T>>
using priority_queue = std::priority_queue<T, co::vector<T>, Compare>;

template<class T, class Alloc = co::stl_allocator<T>>
using list = std::list<T, Alloc>;

template<
    class K, class V,
    class Compare = less<K>,
    class Alloc = co::stl_allocator<std::pair<const K, V>>
> using map = std::map<K, V, Compare, Alloc>;

template<
    class K, class V,
    class Compare = less<K>,
    class Alloc = co::stl_allocator<std::pair<const K, V>>
> using multimap = std::multimap<K, V, Compare, Alloc>;

template<
    class K, class Compare = less<K>,
    class Alloc = co::stl_allocator<K>
> using set = std::set<K, Compare, Alloc>;

template<
    class K, class Compare = less<K>,
    class Alloc = co::stl_allocator<K>
> using multiset = std::multiset<K, Compare, Alloc>;

template<
    class K, class V,
    class Hash = hash<K>,
    class Pred = xx::eq<K>,
    class Alloc = co::stl_allocator<std::pair<const K, V>>
> using hash_map = std::unordered_map<K, V, Hash, Pred, Alloc>;

template<
    class K,
    class Hash = hash<K>,
    class Pred = xx::eq<K>,
    class Alloc = co::stl_allocator<K>
> using hash_set = std::unordered_set<K, Hash, Pred, Alloc>;

namespace xx {

struct Fmt {
    using S = co::string;

    S& fmt(S& s, const char* x, size_t n) {
        s.append('"');
        const char* b = x;
        const char* const e = x + n;
        for (const char* p = b; p < e; ++p) {
            char d;
            const char c = *p;
            switch (c) {
                case '"':  d = '"';  break;
                case '\\': d = '\\'; break;
                case '\0': d = '0';  break;
                case '\r': d = 'r';  break;
                case '\n': d = 'n';  break;
                case '\t': d = 't';  break;
                case '\a': d = 'a';  break;
                case '\b': d = 'b';  break;
                case '\f': d = 'f';  break;
                case '\v': d = 'v';  break;
                default: continue;
            }
            s.append(b, p - b).append('\\').append(d);
            b = p + 1;
        }
        if (b < e) s.append(b, e - b);
        return s.append('"');
    }

    S& fmt(S& s, const char* x) { return fmt(s, x, strlen(x)); }
    S& fmt(S& s, const S& x) { return fmt(s, x.data(), x.size()); }
    S& fmt(S& s, const std::string& x) { return fmt(s, x.data(), x.size()); }
    S& fmt(S& s, char x) { return s << x; }
    S& fmt(S& s, signed char x) { return s << x; }
    S& fmt(S& s, unsigned char x) { return s << x; }
    S& fmt(S& s, bool x) { return s << x; }
    S& fmt(S& s, float x) { return s << x; }
    S& fmt(S& s, double x) { return s << x; }
    S& fmt(S& s, short x) { return s << x; }
    S& fmt(S& s, int x) { return s << x; }
    S& fmt(S& s, long x) { return s << x; }
    S& fmt(S& s, long long x) { return s << x; }
    S& fmt(S& s, unsigned short x) { return s << x; }
    S& fmt(S& s, unsigned int x) { return s << x; }
    S& fmt(S& s, unsigned long x) { return s << x; }
    S& fmt(S& s, unsigned long long x) { return s << x; }
    S& fmt(S& s, const void* x) { return s << x; }

    template<typename K, typename V>
    S& fmt(S& s, const std::pair<K, V>& x) {
        fmt(s, x.first);
        s << ':';
        return fmt(s, x.second);
    }

    template<typename T>
    S& fmt(S& s, const T& beg, const T& end, char c1, char c2) {
        if (beg != end) {
            s << c1;
            for (T it = beg; it != end; ++it) {
                fmt(s, *it);
                s << ',';
            }
            s.back() = c2;
        } else {
            s << c1 << c2;
        }
        return s;
    }

    template<typename T>
    S& fmt(S& s, const co::vector<T>& x) {
        return fmt(s, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    S& fmt(S& s, const std::vector<T>& x) {
        return fmt(s, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    S& fmt(S& s, const co::deque<T>& x) {
        return fmt(s, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    S& fmt(S& s, const std::deque<T>& x) {
        return fmt(s, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    S& fmt(S& s, const co::priority_queue<T>& x) {
        return fmt(s, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    S& fmt(S& s, const std::priority_queue<T>& x) {
        return fmt(s, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    S& fmt(S& s, const co::list<T>& x) {
        return fmt(s, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    S& fmt(S& s, const std::list<T>& x) {
        return fmt(s, x.begin(), x.end(), '[', ']');
    }

    template<typename T>
    S& fmt(S& s, const co::set<T>& x) {
        return fmt(s, x.begin(), x.end(), '{', '}');
    }

    template<typename T>
    S& fmt(S& s, const std::set<T>& x) {
        return fmt(s, x.begin(), x.end(), '{', '}');
    }

    template<typename T>
    S& fmt(S& s, const co::hash_set<T>& x) {
        return fmt(s, x.begin(), x.end(), '{', '}');
    }

    template<typename T>
    S& fmt(S& s, const std::unordered_set<T>& x) {
        return fmt(s, x.begin(), x.end(), '{', '}');
    }

    template<typename K, typename V>
    S& fmt(S& s, const co::map<K, V>& x) {
        return fmt(s, x.begin(), x.end(), '{', '}');
    }

    template<typename K, typename V>
    S& fmt(S& s, const std::map<K, V>& x) {
        return fmt(s, x.begin(), x.end(), '{', '}');
    }

    template<typename K, typename V>
    S& fmt(S& s, const co::hash_map<K, V>& x) {
        return fmt(s, x.begin(), x.end(), '{', '}');
    }

    template<typename K, typename V>
    S& fmt(S& s, const std::unordered_map<K, V>& x) {
        return fmt(s, x.begin(), x.end(), '{', '}');
    }
};

} // xx

template<typename T>
inline co::string& operator<<(co::string& s, const co::vector<T>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename T>
inline co::string& operator<<(co::string& s, const std::vector<T>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename T>
inline co::string& operator<<(co::string& s, const co::deque<T>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename T>
inline co::string& operator<<(co::string& s, const std::deque<T>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename T>
inline co::string& operator<<(co::string& s, const co::priority_queue<T>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename T>
inline co::string& operator<<(co::string& s, const std::priority_queue<T>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename T>
inline co::string& operator<<(co::string& s, const co::list<T>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename T>
inline co::string& operator<<(co::string& s, const std::list<T>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename T>
inline co::string& operator<<(co::string& s, const co::set<T>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename T>
inline co::string& operator<<(co::string& s, const std::set<T>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename T>
inline co::string& operator<<(co::string& s, const co::hash_set<T>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename T>
inline co::string& operator<<(co::string& s, const std::unordered_set<T>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename K, typename V>
inline co::string& operator<<(co::string& s, const std::pair<K, V>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename K, typename V>
inline co::string& operator<<(co::string& s, const co::map<K, V>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename K, typename V>
inline co::string& operator<<(co::string& s, const std::map<K, V>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename K, typename V>
inline co::string& operator<<(co::string& s, const co::hash_map<K, V>& x) {
    return co::xx::Fmt().fmt(s, x);
}

template<typename K, typename V>
inline co::string& operator<<(co::string& s, const std::unordered_map<K, V>& x) {
    return co::xx::Fmt().fmt(s, x);
}

} // co
