#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4200)
#endif
#include "string.h"
#include <initializer_list>

namespace json {
namespace xx {

struct JsonInit {
    JsonInit();
    ~JsonInit() = default;
};

static JsonInit g_json_init;

using P = void*;

struct Array {
    struct _H {
        uint32 cap;
        uint32 size;
        P p[];
    };

    static const size_t N = sizeof(P);
    static const uint32 R = sizeof(_H) / N;

    explicit Array(uint32 cap) noexcept {
        _h = (_H*) co::alloc(N * (R + cap));
        _h->cap = cap;
        _h->size = 0;
    }

    Array() noexcept : Array(1024 - R) {}

    ~Array() noexcept {
        co::free(_h, N * (R + _h->cap));
    }

    P* data() const noexcept { return _h->p; }
    uint32 size() const noexcept { return _h->size; }
    bool empty() const noexcept { return this->size() == 0; }
    void resize(uint32 n) noexcept { _h->size = n; }

    P& back() const noexcept { return _h->p[this->size() - 1]; }
    P& operator[](uint32 i) const noexcept { return _h->p[i]; }

    void push_back(P v) noexcept {
        if (__unlikely(_h->size == _h->cap)) {
            const size_t n = N * _h->cap;
            const size_t o = sizeof(_H) + n;
            _h = (_H*) co::realloc(_h, o, o + n);
            runtime_assert(_h);
            _h->cap <<= 1;
        }
        _h->p[_h->size++] = v;
    }

    P pop_back() noexcept {
        return _h->p[--_h->size];
    }

    void remove(uint32 i) noexcept {
        if (i != --_h->size) _h->p[i] = _h->p[_h->size];
    }

    void remove_pair(uint32 i) noexcept {
        if (i != (_h->size -= 2)) {
            _h->p[i] = _h->p[_h->size];
            _h->p[i + 1] = _h->p[_h->size + 1];
        }
    }

    void erase(uint32 i) noexcept {
        if (i != --_h->size) {
            ::memmove(_h->p + i, _h->p + i + 1, (_h->size - i) * N);
        }
    }

    void erase_pair(uint32 i) noexcept {
        if (i != (_h->size -= 2)) {
            ::memmove(_h->p + i, _h->p + i + 2, (_h->size - i) * N);
        }
    }

    _H* _h;
};

void* alloc_header();
char* make_string(const void* p, size_t n);

} // xx

struct any {
    enum {
        t_null = 0,
        t_bool = 1,
        t_int = 2,
        t_double = 4,
        t_string = 8,
        t_array = 16,
        t_object = 32,
    };

    struct _obj_t {};
    struct _arr_t {};
    using ao_t = xx::Array;

    struct _H {
        _H(bool v) noexcept : type(t_bool), b(v) {}
        _H(int64 v) noexcept : type(t_int), i(v) {}
        _H(double v) noexcept : type(t_double), d(v) {}
        _H(_obj_t) noexcept : type(t_object), p(0) {}
        _H(_arr_t) noexcept : type(t_array), p(0) {}
        _H(const char* p) noexcept : _H(p, strlen(p)) {}
        _H(const void* p, size_t n) noexcept : type(t_string), size((uint32)n) {
            s = xx::make_string(p, n);
        }
        ~_H() {};

        uint32 type;
        uint32 size;  // size of string
        union {
            bool b;   // for bool
            int64 i;  // for int
            double d; // for double
            char* s;  // for string
            void* p;  // for array and object
            ao_t ao;
        };
    };

    constexpr any() noexcept : _h(0) {}
    constexpr any(decltype(nullptr)) noexcept : _h(0) {}
    any(any&& v) noexcept : _h(v._h) { v._h = 0; }
    any(any& v) noexcept : _h(v._h) { v._h = 0; }
    ~any() { if (_h) this->reset(); }

    any(const any&) = delete;
    void operator=(const any&) = delete;

    any& operator=(any&& v) noexcept {
        if (&v != this) {
            if (_h) this->reset();
            _h = v._h;
            v._h = 0;
        }
        return *this;
    }

    // after this operation, v will be moved and becomes null
    any& operator=(any& v) noexcept {
        return this->operator=(std::move(v));
    }

    // make a duplicate 
    any dup() const noexcept {
        any r;
        r._h = (_H*)this->_dup();
        return r;
    }

    any(bool v) noexcept   : _h(new(xx::alloc_header()) _H(v)) {}
    any(double v) noexcept : _h(new(xx::alloc_header()) _H(v)) {}
    any(int64 v) noexcept  : _h(new(xx::alloc_header()) _H(v)) {}
    any(int32 v) noexcept  : any((int64)v) {}
    any(uint32 v) noexcept : any((int64)v) {}
    any(uint64 v) noexcept : any((int64)v) {}
    any(const void* p, size_t n) noexcept : _h(new(xx::alloc_header()) _H(p, n)) {}
    any(const char* s) noexcept : any(s, strlen(s)) {}
    any(const co::string& s) noexcept : any(s.data(), s.size()) {}
    any(const std::string& s) noexcept : any(s.data(), s.size()) {}
    any(_obj_t) noexcept : _h(new(xx::alloc_header()) _H(_obj_t())) {}
    any(_arr_t) noexcept : _h(new(xx::alloc_header()) _H(_arr_t())) {}
    any(std::initializer_list<any> v) noexcept;

    int type() const noexcept { return _h ? _h->type : t_null; }
    bool is_null() const noexcept { return _h == nullptr; }
    bool is_bool() const noexcept { return _h && (_h->type == t_bool); }
    bool is_int() const noexcept { return _h && (_h->type == t_int); }
    bool is_double() const noexcept { return _h && (_h->type == t_double); }
    bool is_string() const noexcept { return _h && (_h->type == t_string); }
    bool is_array() const noexcept { return _h && (_h->type == t_array); }
    bool is_object() const noexcept { return _h && (_h->type == t_object); }

    // try to get a bool value
    //   - int or double type, 0 -> false, !0 -> true
    //   - string type, "true" or "1" -> true, otherwise -> false
    //   - other non-bool types, -> false
    bool as_bool() const noexcept {
        if (_h) {
            switch (_h->type) {
                case t_bool:   return _h->b;
                case t_int:    return _h->i != 0;
                case t_string: return co::stob(_h->s);
                case t_double: return _h->d != 0;
            }
        }
        return false;
    }

    // try to get an integer value
    //   - string or double type, convert to integer
    //   - bool type, true -> 1, false -> 0
    //   - other non-int types, -> 0
    int64 as_int64() const noexcept {
        if (_h) {
            switch (_h->type) {
                case t_int:    return _h->i;
                case t_string: return co::stoi64(_h->s);
                case t_double: return (int64)_h->d;
                case t_bool:   return _h->b ? 1 : 0;
            }
        }
        return 0;
    }

    int32 as_int32() const noexcept { return (int32) this->as_int64(); }
    int as_int() const noexcept { return (int) this->as_int64(); }

    // try to get a double value
    //   - string or integer type, convert to double
    //   - bool type, true -> 1, false -> 0
    //   - other non-double types, -> 0
    double as_double() const noexcept {
        if (_h) {
            switch (_h->type) {
                case t_double: return _h->d;
                case t_int:    return (double)_h->i;
                case t_string: return co::stod(_h->s);
                case t_bool:   return _h->b ? 1 : 0;
            }
        }
        return 0;
    }

    // returns a c-style string, null-terminated, for non-string types, returns "".
    const char* as_c_str() const noexcept {
        return this->is_string() ? _h->s : "";
    }

    // returns a co::string
    //   - null -> ""
    //   - other non-string types -> any::str().
    co::string as_string() const noexcept {
        return _h ? (_h->type == t_string ? co::string(_h->s, _h->size) : this->str()) : co::string();
    }

    // get JSON by index or key.
    //   - It is a read-only operation.
    //   - If the index is not valid or the key does not exist, 
    //     the return value is a reference to null object.
    any& get() const noexcept { return *(any*)this; }
    any& get(uint32 i) const noexcept {
        return i < this->array_size() ? (any&)_h->ao[i] : _null();
    }
    any& get(int i) const noexcept { return this->get((uint32)i); }
    any& get(const char* key) const noexcept;

    template<typename T, typename ...X>
    inline any& get(T&& v, X&& ... x) const noexcept {
        auto& r = this->get(std::forward<T>(v));
        return !r.is_null() ? r.get(std::forward<X>(x)...) : r;
    }

    // set value for JSON.
    //   - The last parameter is the value, other parameters are index or key.
    //   - eg.
    //     any x;
    //     x.set("a", "b", 0, 3);  // x-> {"a": {"b": [3]}}
    template<typename T>
    inline any& set(T&& v) noexcept { return *this = any(std::forward<T>(v)); }

    template<typename A, typename B,  typename ...X>
    inline any& set(A&& a, B&& b, X&& ... x) noexcept {
        auto& r = this->_set(std::forward<A>(a));
        return r.set(std::forward<B>(b), std::forward<X>(x)...);
    }

    // push v to an array.
    // if JSON is not array, it will be reset to array.
    any& push_back(any&& v) noexcept {
        if (this->is_array()) goto _1;

        this->reset();
        _h = new(xx::alloc_header()) _H(_arr_t());
        new(&_h->ao) ao_t(8);
        goto _2;

    _1:
        if (_h->p) goto _2;
        new(&_h->ao) ao_t(8);

    _2:
        _h->ao.push_back(v._h);
        v._h = 0;
        return *this;
    }

    any& push_back(any& v) noexcept {
        return this->push_back(std::move(v));
    }

    // remove the ith element from an array
    // the last element will be moved to the ith place
    void remove(uint32 i) noexcept {
        if (i < this->array_size()) {
            ((any&)_h->ao[i]).reset();
            _h->ao.remove(i);
        }
    }

    void remove(int i) noexcept { this->remove((uint32)i); }
    void remove(const char* key) noexcept;

    // erase the ith element from an array
    void erase(uint32 i) noexcept {
        if (i < this->array_size()) {
            ((any&)_h->ao[i]).reset();
            _h->ao.erase(i);
        }
    }

    void erase(int i) noexcept { this->erase((uint32)i); }
    void erase(const char* key) noexcept;

    any& operator[](uint32 i) noexcept { return this->get(i); }
    any& operator[](int i) noexcept { return this->get((uint32)i); }
    const any& operator[](uint32 i) const noexcept { return this->get(i); }
    const any& operator[](int i) const noexcept { return this->get((uint32)i); }

    // for array and object, return number of the elements.
    // for string, return the length.
    // for other types, return 0.
    uint32 size() const noexcept {
        if (_h) {
            switch (_h->type) {
                case t_array:  return _h->p ? _h->ao.size() : 0;
                case t_object: return _h->p ? (_h->ao.size() >> 1) : 0;
                case t_string: return _h->size;
            }
        }
        return 0;
    }

    bool empty() const noexcept { return this->size() == 0; }

    uint32 array_size() const noexcept {
        return (this->is_array() && _h->p) ? _h->ao.size() : 0;
    }

    uint32 object_size() const noexcept {
        return (this->is_object() && _h->p) ? (_h->ao.size() >> 1) : 0;
    }

    uint32 string_size() const noexcept {
        return this->is_string() ? _h->size : 0;
    }

    // push key-value to the back of an object, key may be repeated.
    // if JSON is not object, it will be reset to object.
    any& add_member(const char* key, any&& v) noexcept {
        if (this->is_object()) goto _1;

        this->reset();
        _h = new(xx::alloc_header()) _H(_obj_t());
        new(&_h->ao) ao_t(16);
        goto _2;

    _1:
        if (_h->p) goto _2;
        new(&_h->ao) ao_t(16);

    _2:
        _h->ao.push_back(xx::make_string(key, strlen(key))); // key
        _h->ao.push_back(v._h);
        v._h = 0;
        return *this;
    }

    any& add_member(const char* key, any& v) noexcept {
        return this->add_member(key, std::move(v));
    }

    bool has_member(const char* key) const noexcept;

    const any& operator[](const char* key) const noexcept { return this->get(key); }
    any& operator[](const char* key) noexcept { return this->_set(key); }

    struct iterator {
        typedef void* T;
        iterator(T* p, T* e, uint32 step) noexcept : _p(p), _e(e), _step(step) {}

        enum _End { _end };
        bool operator!=(_End) const noexcept { return _p != _e; }
        bool operator==(_End) const noexcept { return _p == _e; }
        iterator& operator++() noexcept { _p += _step; return *this; }
        iterator operator++(int) = delete;

        const char* key() const noexcept { return (const char*)_p[0]; }
        any& value() const noexcept { return (any&)_p[1]; }
        any& operator*() const noexcept { return (any&)_p[0]; }

    private:
        T* _p;
        T* _e;
        uint32 _step;
    };

    // the begin iterator.
    //   - If any is not array or object type, the return value is equal to the 
    //     end iterator.
    iterator begin() const noexcept {
        if (_h && _h->p && (_h->type & (t_array | t_object))) {
            static_assert(t_array == 16 && t_object == 32, "");
            auto& a = _h->ao;
            return iterator(a.data(), a.data() + a.size(), _h->type >> 4);
        }
        return iterator(0, 0, 0);
    }

    // a fake end iterator
    const iterator::_End end() const noexcept { return iterator::_end; }

    // Stringify.
    //   - str() converts JSON to minified string.
    //   - dbg() like the str(), but will truncate long string (> 512 bytes).
    //   - pretty() converts JSON to human readable string.
    //   - mdp: max decimal places for float point numbers.
    co::string str(int mdp=16)    const noexcept { co::string s(256); this->_json2str(s, false, mdp); return s; }
    co::string dbg(int mdp=16)    const noexcept { co::string s(256); this->_json2str(s, true, mdp); return s; }
    co::string pretty(int mdp=16) const noexcept { co::string s(256); this->_json2pretty(s, 4, 4, mdp); return s; }

    // Parse any from string, inverse to stringify.
    bool parse_from(const char* s, size_t n) noexcept;
    bool parse_from(const char* s) noexcept        { return this->parse_from(s, strlen(s)); }
    bool parse_from(const co::string& s) noexcept  { return this->parse_from(s.data(), s.size()); }
    bool parse_from(const std::string& s) noexcept { return this->parse_from(s.data(), s.size()); }

    void reset() noexcept;
    void swap(any& v) noexcept { auto h = _h; _h = v._h; v._h = h; }
    void swap(any&& v) noexcept { v.swap(*this); }

    bool operator==(bool v) const noexcept { return this->is_bool() && _h->b == v; }
    bool operator==(double v) const noexcept { return this->is_double() && _h->d == v; }
    bool operator==(int64 v) const noexcept { return this->is_int() && _h->i == v; }
    bool operator==(int v) const noexcept { return this->operator==((int64)v); }
    bool operator==(uint32 v) const noexcept { return this->operator==((int64)v); }
    bool operator==(uint64 v) const noexcept { return this->operator==((int64)v); }
    bool operator==(const char* v) const noexcept { return this->is_string() && strcmp(_h->s, v) == 0; }
    bool operator==(const co::string& v) const noexcept { return this->is_string() && v == _h->s; }
    bool operator==(const std::string& v) const noexcept { return this->is_string() && v == _h->s; }
    bool operator!=(bool v) const noexcept { return !this->operator==(v); }
    bool operator!=(double v) const noexcept { return !this->operator==(v); }
    bool operator!=(int64 v) const noexcept { return !this->operator==(v); }
    bool operator!=(int v) const noexcept { return !this->operator==(v); }
    bool operator!=(uint32 v) const noexcept { return !this->operator==(v); }
    bool operator!=(uint64 v) const noexcept { return !this->operator==(v); }
    bool operator!=(const char* v) const noexcept { return !this->operator==(v); }
    bool operator!=(const co::string& v) const noexcept { return !this->operator==(v); }
    bool operator!=(const std::string& v) const noexcept { return !this->operator==(v); }

    void* _dup() const;
    static any& _null();
    any& _set(uint32 i);
    any& _set(int i) { return this->_set((uint32)i); }
    any& _set(const char* key);
    co::string& _json2str(co::string& s, bool debug, int mdp=16) const;
    co::string& _json2pretty(co::string& s, int indent, int n, int mdp=16) const;

    _H* _h;
};

using Json = any;

// make an empty array
inline any array() noexcept { return any(any::_arr_t()); }

// make an array from initializer_list
any array(std::initializer_list<any> v) noexcept;

// make an empty object
inline any object() noexcept { return any(any::_obj_t()); }

// make an object from initializer_list
any object(std::initializer_list<any> v) noexcept;

inline any parse(const char* s, size_t n) noexcept {
    any r;
    if (r.parse_from(s, n)) return r;
    r.reset();
    return r;
}

inline any parse(const char* s) noexcept        { return parse(s, strlen(s)); }
inline any parse(const co::string& s) noexcept  { return parse(s.data(), s.size()); }
inline any parse(const std::string& s) noexcept { return parse(s.data(), s.size()); }

inline co::string& operator<<(co::string& s, const json::any& x) noexcept {
    return x._json2str(s, false);
}

} // json
