#pragma once

#ifdef _MSC_VER
#pragma warning (disable:4200)
#endif

#include "fastream.h"
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

    static const uint32 R = sizeof(_H) / sizeof(T);

    explicit Array(uint32 cap) {
        _h = (_H*) co::alloc(sizeof(_H) + sizeof(T) * cap);
        _h->cap = cap;
        _h->size = 0;
    }

    Array() : Array(1024 - R) {}

    ~Array() {
        co::free(_h, sizeof(_H) + sizeof(T) * _h->cap);
    }

    T* data() const { return _h->p; }
    //uint32 capicity() const { return _h->cap; }
    uint32 size() const { return _h->size; }
    bool empty() const { return this->size() == 0; }
    void resize(uint32 n) { _h->size = n; }

    T& back() const { return _h->p[this->size() - 1]; }
    T& operator[](uint32 i) const { return _h->p[i]; }

    void push_back(T v) {
        if (_h->size == _h->cap) {
            const uint32 n = sizeof(T) * _h->cap;
            const uint32 o = sizeof(_H) + n;
            _h = (_H*) co::realloc(_h, o, o + n); assert(_h);
            _h->cap <<= 1;
        }
        _h->p[_h->size++] = v;
    }

    T pop_back() {
        return _h->p[--_h->size];
    }

    void reset() {
        _h->size = 0;
        if (_h->cap > 8192) {
            co::free(_h, sizeof(_H) + sizeof(T) * _h->cap);
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

    bool is_null() const { return _h == 0; }
    bool is_bool() const { return _h && (_h->type & t_bool); }
    bool is_int() const { return _h && (_h->type & t_int); }
    bool is_double() const { return _h && (_h->type & t_double); }
    bool is_string() const { return _h && (_h->type & t_string); }
    bool is_array() const { return _h && (_h->type & t_array); }
    bool is_object() const { return _h && (_h->type & t_object); }

    bool as_bool() const { return this->is_bool() ? _h->b : false; }
    int64 as_int64() const { return this->is_int() ? _h->i : 0; }
    int32 as_int32() const { return (int32) this->as_int64(); }
    int as_int() const { return (int) this->as_int64(); }
    double as_double() const { return this->is_double() ? _h->d : 0; }
    const char* as_string() const { return this->is_string() ? _h->s : ""; }

    inline Json& get() const { return *(Json*)this; }
    Json& get(uint32 i) const;
    Json& get(int i) const { return this->get((uint32)i); }
    Json& get(const char* key) const;

    template <class T,  class ...X>
    inline Json& get(T&& v, X&& ... x) const {
        auto& r = this->get(std::forward<T>(v));
        if (r.is_null()) return r;
        return r.get(std::forward<X>(x)...);
    }

    void push_back(Json&& v) {
        if (_h) {
            assert(_h->type & t_array);
            if (unlikely(!_h->p)) new(&_h->p) xx::Array(8);
        } else {
            _h = new(xx::alloc()) _H(_arr_t());
            new(&_h->p) xx::Array(8);
        }
        _array().push_back(v._h);
        v._h = 0;
    }

    void push_back(Json& v) {
        this->push_back(std::move(v));
    }

    Json& operator[](uint32 i) const {
        assert(this->is_array() && !_array().empty());
        return *(Json*)&_array()[i];
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

    void add_member(const char* key, Json&& v) {
        if (_h) {
            assert(_h->type & t_object);
            if (unlikely(!_h->p)) new(&_h->p) xx::Array(16);
        } else {
            _h = new(xx::alloc()) _H(_obj_t());
            new(&_h->p) xx::Array(16);
        }
        _array().push_back(xx::alloc_string(key, strlen(key))); // key
        _array().push_back(v._h);
        v._h = 0;
    }

    void add_member(const char* key, Json& v) {
        this->add_member(key, std::move(v));
    }

    bool has_member(const char* key) const;
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
        Json& value() const { return *(Json*)&_p[1]; }
        Json& operator*() const { return *(Json*)&_p[0]; }

      private:
        T* _p;
        T* _e;
        uint32 _step;
    };

    iterator begin() const {
        if (unlikely(!_h || !_h->p)) return iterator(0, 0, 0);
        assert(_h->type & (t_array | t_object));
        static_assert(t_array == 16 && t_object == 32, "");
        auto& a = _array();
        return iterator(a.data(), a.data() + a.size(), _h->type >> 4);
    }

    const iterator::End& end() const { return iterator::end(); }

    // Stringify.
    //   - str() converts Json to minified string.
    //   - dbg() like the str(), but will truncate long string type (> 512 bytes).
    //   - pretty() converts Json to human readable string.
    fastream& str(fastream& s)      const { return this->_json2str(s, false); }
    fastring& str(fastring& s)      const { return (fastring&)this->str(*(fastream*)&s); }
    fastream& dbg(fastream& s)      const { return this->_json2str(s, true); }
    fastring& dbg(fastring& s)      const { return (fastring&)this->dbg(*(fastream*)&s); }
    fastream& pretty(fastream& s)   const { return this->_json2pretty(s, 4, 4); }
    fastring& pretty(fastring& s)   const { return (fastring&)this->pretty(*(fastream*)&s); }
    fastring str(uint32 cap=256)    const { fastring s(cap); this->str(s); return s; }
    fastring dbg(uint32 cap=256)    const { fastring s(cap); this->dbg(s); return s; }
    fastring pretty(uint32 cap=256) const { fastring s(cap); this->pretty(s); return s; }

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
    xx::Array& _array() const { return *((xx::Array*)&_h->p); }
    fastream& _json2str(fastream& fs, bool debug) const;
    fastream& _json2pretty(fastream& fs, int indent, int n) const;

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

typedef json::Json Json;

inline fastream& operator<<(fastream& fs, const json::Json& x) { return x.dbg(fs); }
