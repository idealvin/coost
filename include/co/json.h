#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4200)
#endif

#include "fastream.h"
#include "str.h"
#include <initializer_list>

namespace json {
namespace xx {

class Array {
  public:
    typedef void* T;
    struct _H {
        uint32 cap;
        uint32 size;
        T p[];
    };

    static const size_t N = sizeof(T);
    static const uint32 R = sizeof(_H) / N;

    explicit Array(uint32 cap) {
        _h = (_H*) co::alloc(N * (R + cap));
        _h->cap = cap;
        _h->size = 0;
    }

    Array() : Array(1024 - R) {}

    ~Array() {
        co::free(_h, N * (R + _h->cap));
    }

    T* data() const { return _h->p; }
    uint32 size() const { return _h->size; }
    bool empty() const { return this->size() == 0; }
    void resize(uint32 n) { _h->size = n; }

    T& back() const { return _h->p[this->size() - 1]; }
    T& operator[](uint32 i) const { return _h->p[i]; }

    void push_back(T v) {
        if (_h->size == _h->cap) {
            const size_t n = N * _h->cap;
            const size_t o = sizeof(_H) + n;
            _h = (_H*) co::realloc(_h, o, o + n); assert(_h);
            _h->cap <<= 1;
        }
        _h->p[_h->size++] = v;
    }

    T pop_back() {
        return _h->p[--_h->size];
    }

    void remove(uint32 i) {
        if (i != --_h->size) _h->p[i] = _h->p[_h->size];
    }

    void remove_pair(uint32 i) {
        if (i != (_h->size -= 2)) {
            _h->p[i] = _h->p[_h->size];
            _h->p[i + 1] = _h->p[_h->size + 1];
        }
    }

    void erase(uint32 i) {
        if (i != --_h->size) {
            memmove(_h->p + i, _h->p + i + 1, (_h->size - i) * N);
        }
    }

    void erase_pair(uint32 i) {
        if (i != (_h->size -= 2)) {
            memmove(_h->p + i, _h->p + i + 2, (_h->size - i) * N);
        }
    }

    void reset() {
        _h->size = 0;
        if (_h->cap > 8192) {
            co::free(_h, N * (R + _h->cap));
            new(this) Array();
        }
    }

  private:
    _H* _h;
};

__coapi void* alloc();
__coapi char* alloc_string(const void* p, size_t n);

} // xx

class __coapi Json {
  public:
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

    struct _H {
        _H(bool v) noexcept : type(t_bool), b(v) {}
        _H(int64 v) noexcept : type(t_int), i(v) {}
        _H(double v) noexcept : type(t_double), d(v) {}
        _H(_obj_t) noexcept : type(t_object), p(0) {}
        _H(_arr_t) noexcept : type(t_array), p(0) {}

        _H(const char* p) : _H(p, strlen(p)) {}
        _H(const void* p, size_t n) : type(t_string), size((uint32)n) {
            s = xx::alloc_string(p, n);
        }

        uint32 type;
        uint32 size;  // size of string
        union {
            bool b;   // for bool
            int64 i;  // for int
            double d; // for double
            char* s;  // for string
            void* p;  // for array and object
        };
    };

    Json() noexcept : _h(0) {}
    Json(decltype(nullptr)) noexcept : _h(0) {}
    Json(Json&& v) noexcept : _h(v._h) { v._h = 0; }
    Json(Json& v) noexcept : _h(v._h) { v._h = 0; }
    ~Json() { if (_h) this->reset(); }

    Json(const Json& v) = delete;
    void operator=(const Json&) = delete;

    Json& operator=(Json&& v) {
        if (&v != this) {
            if (_h) this->reset();
            _h = v._h;
            v._h = 0;
        }
        return *this;
    }

    // after this operation, v will be moved and becomes null
    Json& operator=(Json& v) {
        return this->operator=(std::move(v));
    }

    // make a duplicate 
    Json dup() const {
        Json r;
        r._h = (_H*)this->_dup();
        return r;
    }

    Json(bool v)   : _h(new(xx::alloc()) _H(v)) {}
    Json(double v) : _h(new(xx::alloc()) _H(v)) {}
    Json(int64 v)  : _h(new(xx::alloc()) _H(v)) {}
    Json(int32 v)  : Json((int64)v) {}
    Json(uint32 v) : Json((int64)v) {}
    Json(uint64 v) : Json((int64)v) {}

    // for string type
    Json(const void* p, size_t n) : _h(new(xx::alloc()) _H(p, n)) {}
    Json(const char* s) : Json(s, strlen(s)) {}
    Json(const fastring& s) : Json(s.data(), s.size()) {}
    Json(const std::string& s) : Json(s.data(), s.size()) {}

    Json(_obj_t) : _h(new(xx::alloc()) _H(_obj_t())) {}
    Json(_arr_t) : _h(new(xx::alloc()) _H(_arr_t())) {}

    // make Json from initializer_list
    Json(std::initializer_list<Json> v);

    int type() const { return _h ? _h->type : t_null; }
    bool is_null() const { return _h == 0; }
    bool is_bool() const { return _h && (_h->type & t_bool); }
    bool is_int() const { return _h && (_h->type & t_int); }
    bool is_double() const { return _h && (_h->type & t_double); }
    bool is_string() const { return _h && (_h->type & t_string); }
    bool is_array() const { return _h && (_h->type & t_array); }
    bool is_object() const { return _h && (_h->type & t_object); }

    // try to get a bool value
    //   - int or double type, 0 -> false, !0 -> true
    //   - string type, "true" or "1" -> true, otherwise -> false
    //   - other non-bool types, -> false
    bool as_bool() const {
        if (_h) {
            switch (_h->type) {
              case t_bool:   return _h->b;
              case t_int:    return _h->i != 0;
              case t_string: return str::to_bool(_h->s);
              case t_double: return _h->d != 0;
            }
        }
        return false;
    }

    // try to get an integer value
    //   - string or double type, convert to integer
    //   - bool type, true -> 1, false -> 0
    //   - other non-int types, -> 0
    int64 as_int64() const {
        if (_h) {
            switch (_h->type) {
              case t_int:    return _h->i;
              case t_string: return str::to_int64(_h->s);
              case t_double: return (int64)_h->d;
              case t_bool:   return _h->b ? 1 : 0;
            }
        }
        return 0;
    }

    int32 as_int32() const { return (int32) this->as_int64(); }
    int as_int() const { return (int) this->as_int64(); }

    // try to get a double value
    //   - string or integer type, convert to double
    //   - bool type, true -> 1, false -> 0
    //   - other non-double types, -> 0
    double as_double() const {
        if (_h) {
            switch (_h->type) {
              case t_double: return _h->d;
              case t_int:    return (double)_h->i;
              case t_string: return str::to_double(_h->s);
              case t_bool:   return _h->b ? 1 : 0;
            }
        }
        return 0;
    }

    // returns a c-style string, null-terminated.
    // for non-string types, returns an empty string.
    const char* as_c_str() const {
        return this->is_string() ? _h->s : "";
    }

    // returns a fastring.
    // for non-string types, it is equal to Json::str().
    fastring as_string() const {
        return this->is_string() ? fastring(_h->s, _h->size) : this->str();
    }

    // get Json by index or key.
    //   - It is a read-only operation.
    //   - If the index is not in a valid range or the key does not exist, 
    //     the return value is a reference to a null object.
    Json& get() const { return *(Json*)this; }
    Json& get(uint32 i) const;
    Json& get(int i) const { return this->get((uint32)i); }
    Json& get(const char* key) const;

    template <class T,  class ...X>
    inline Json& get(T&& v, X&& ... x) const {
        auto& r = this->get(std::forward<T>(v));
        return r.is_null() ? r : r.get(std::forward<X>(x)...);
    }

    // set value for Json.
    //   - The last parameter is the value, other parameters are index or key.
    //   - eg.
    //     Json x;
    //     x.set("a", "b", 0, 3);  // x-> {"a": {"b": [3]}}
    template <class T>
    inline Json& set(T&& v) { return *this = Json(std::forward<T>(v)); }

    template <class A, class B,  class ...X>
    inline Json& set(A&& a, B&& b, X&& ... x) {
        auto& r = this->_set(std::forward<A>(a));
        return r.set(std::forward<B>(b), std::forward<X>(x)...);
    }

    // push v to an array.
    // if the Json calling this method is not an array, it will be reset to an array.
    Json& push_back(Json&& v) {
        if (_h && (_h->type & t_array)) {
            if (unlikely(!_h->p)) new(&_h->p) xx::Array(8);
        } else {
            this->reset();
            _h = new(xx::alloc()) _H(_arr_t());
            new(&_h->p) xx::Array(8);
        }
        _array().push_back(v._h);
        v._h = 0;
        return *this;
    }

    Json& push_back(Json& v) {
        return this->push_back(std::move(v));
    }

    // remove the ith element from an array
    // the last element will be moved to the ith place
    void remove(uint32 i) {
        if (this->is_array() && i < this->array_size()) {
            ((Json&)_array()[i]).reset();
            _array().remove(i);
        }
    }

    void remove(int i) { this->remove((uint32)i); }
    void remove(const char* key);

    // erase the ith element from an array
    void erase(uint32 i) {
        if (this->is_array() && i < this->array_size()) {
            ((Json&)_array()[i]).reset();
            _array().erase(i);
        }
    }

    void erase(int i) { this->erase((uint32)i); }
    void erase(const char* key);

    // it is better to use get() instead of this method.
    Json& operator[](uint32 i) const {
        assert(this->is_array() && !_array().empty());
        return (Json&)_array()[i];
    }

    Json& operator[](int i) const {
        return this->operator[]((uint32)i);
    }

    bool operator==(bool v) const { return this->is_bool() && _h->b == v; }
    bool operator==(double v) const { return this->is_double() && _h->d == v; }
    bool operator==(int64 v) const { return this->is_int() && _h->i == v; }
    bool operator==(int v) const { return this->operator==((int64)v); }
    bool operator==(uint32 v) const { return this->operator==((int64)v); }
    bool operator==(uint64 v) const { return this->operator==((int64)v); }
    bool operator==(const char* v) const { return this->is_string() && strcmp(_h->s, v) == 0; }
    bool operator==(const fastring& v) const { return this->is_string() && v == _h->s; }
    bool operator==(const std::string& v) const { return this->is_string() && v == _h->s; }
    bool operator!=(bool v) const { return !this->operator==(v); }
    bool operator!=(double v) const { return !this->operator==(v); }
    bool operator!=(int64 v) const { return !this->operator==(v); }
    bool operator!=(int v) const { return !this->operator==(v); }
    bool operator!=(uint32 v) const { return !this->operator==(v); }
    bool operator!=(uint64 v) const { return !this->operator==(v); }
    bool operator!=(const char* v) const { return !this->operator==(v); }
    bool operator!=(const fastring& v) const { return !this->operator==(v); }
    bool operator!=(const std::string& v) const { return !this->operator==(v); }

    // for array and object, return number of the elements.
    // for string, return the length.
    // for other types, return 0.
    uint32 size() const {
        if (_h) {
            switch (_h->type) {
              case t_array:
                return _h->p ? _array().size() : 0;
              case t_object:
                return _h->p ? (_array().size() >> 1) : 0;
              case t_string:
                return _h->size;
            }
        }
        return 0;
    }

    bool empty() const { return this->size() == 0; }

    uint32 array_size() const {
        if (_h && (_h->type & t_array)) {
            return _h->p ? _array().size() : 0;
        }
        return 0;
    }

    uint32 object_size() const {
        if (_h && (_h->type & t_object)) {
            return _h->p ? (_array().size() >> 1) : 0;
        }
        return 0;
    }

    uint32 string_size() const {
        return (_h && (_h->type & t_string)) ? _h->size : 0;
    }

    // push key-value to the back of an object, key may be repeated.
    // if the Json calling this method is not an object, it will be reset to an object.
    Json& add_member(const char* key, Json&& v) {
        if (_h && (_h->type & t_object)) {
            if (unlikely(!_h->p)) new(&_h->p) xx::Array(16);
        } else {
            this->reset();
            _h = new(xx::alloc()) _H(_obj_t());
            new(&_h->p) xx::Array(16);
        }
        _array().push_back(xx::alloc_string(key, strlen(key))); // key
        _array().push_back(v._h);
        v._h = 0;
        return *this;
    }

    Json& add_member(const char* key, Json& v) {
        this->add_member(key, std::move(v));
        return *this;
    }

    bool has_member(const char* key) const;

    // it is better to use get(key) instead of this method.
    Json& operator[](const char* key) const;

    class iterator {
      public:
        typedef void* T;
        iterator(T* p, T* e, uint32 step) : _p(p), _e(e), _step(step) {}

        struct End {}; // fake end
        static const End& end() { static End kEnd; return kEnd; }

        bool operator!=(const End&) const { return _p != _e; }
        bool operator==(const End&) const { return _p == _e; }
        iterator& operator++() { _p += _step; return *this; }
        iterator operator++(int) = delete;

        const char* key() const { return (const char*)_p[0]; }
        Json& value() const { return (Json&)_p[1]; }
        Json& operator*() const { return (Json&)_p[0]; }

      private:
        T* _p;
        T* _e;
        uint32 _step;
    };

    // the begin iterator.
    //   - If Json is not array or object type, the return value is equal to the 
    //     end iterator.
    iterator begin() const {
        if (_h && _h->p && (_h->type & (t_array | t_object))) {
            static_assert(t_array == 16 && t_object == 32, "");
            auto& a = _array();
            return iterator(a.data(), a.data() + a.size(), _h->type >> 4);
        }
        return iterator(0, 0, 0);
    }

    // a fake end iterator
    const iterator::End& end() const { return iterator::end(); }

    // Stringify.
    //   - str() converts Json to minified string.
    //   - dbg() like the str(), but will truncate long string type (> 512 bytes).
    //   - pretty() converts Json to human readable string.
    //   - mdp: max decimal places for float point numbers.
    fastream& str(fastream& s, int mdp=16)    const { return this->_json2str(s, false, mdp); }
    fastring& str(fastring& s, int mdp=16)    const { return (fastring&)this->str((fastream&)s, mdp); }
    fastream& dbg(fastream& s, int mdp=16)    const { return this->_json2str(s, true, mdp); }
    fastring& dbg(fastring& s, int mdp=16)    const { return (fastring&)this->dbg((fastream&)s, mdp); }
    fastream& pretty(fastream& s, int mdp=16) const { return this->_json2pretty(s, 4, 4, mdp); }
    fastring& pretty(fastring& s, int mdp=16) const { return (fastring&)this->pretty((fastream&)s, mdp); }
    fastring str(int mdp=16)    const { fastring s(256); this->str(s, mdp); return s; }
    fastring dbg(int mdp=16)    const { fastring s(256); this->dbg(s, mdp); return s; }
    fastring pretty(int mdp=16) const { fastring s(256); this->pretty(s, mdp); return s; }

    // Parse Json from string, inverse to stringify.
    bool parse_from(const char* s, size_t n);
    bool parse_from(const char* s)        { return this->parse_from(s, strlen(s)); }
    bool parse_from(const fastring& s)    { return this->parse_from(s.data(), s.size()); }
    bool parse_from(const std::string& s) { return this->parse_from(s.data(), s.size()); }

    void reset();
    void swap(Json& v) noexcept { auto h = _h; _h = v._h; v._h = h; }
    void swap(Json&& v) noexcept { v.swap(*this); }

  private:
    friend class Parser;
    void* _dup() const;
    xx::Array& _array() const { return (xx::Array&)_h->p; }
    Json& _set(uint32 i);
    Json& _set(int i) { return this->_set((uint32)i); }
    Json& _set(const char* key);
    fastream& _json2str(fastream& fs, bool debug, int mdp) const;
    fastream& _json2pretty(fastream& fs, int indent, int n, int mdp) const;

  private:
    _H* _h;
};

// make an empty array
inline Json array() { return Json(Json::_arr_t()); }

// make an array from initializer_list
__coapi Json array(std::initializer_list<Json> v);

// make an empty object
inline Json object() { return Json(Json::_obj_t()); }

// make an object from initializer_list
__coapi Json object(std::initializer_list<Json> v);

inline Json parse(const char* s, size_t n) {
    Json r;
    if (r.parse_from(s, n)) return r;
    r.reset();
    return r;
}

inline Json parse(const char* s)        { return parse(s, strlen(s)); }
inline Json parse(const fastring& s)    { return parse(s.data(), s.size()); }
inline Json parse(const std::string& s) { return parse(s.data(), s.size()); }

} // json

namespace co {
using Json = json::Json;
} // co

inline fastream& operator<<(fastream& fs, const json::Json& x) { return x.dbg(fs); }
