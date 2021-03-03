#pragma once

#include "array.h"
#include "atomic.h"
#include "fastream.h"

namespace json {

class Value {
  public:
    enum Type {
        kBool = 1,
        kInt = 2,
        kDouble = 4,
        kString = 8,
        kArray = 16,
        kObject = 32,
    };

    struct JArray {};
    struct JObject {};
    typedef const char* Key;

    class Jalloc {
      public:
        Jalloc() = default;
        ~Jalloc();

        static Jalloc* instance() {
            static __thread Jalloc* jalloc = 0;
            return jalloc ? jalloc : (jalloc = new Jalloc);
        }

        void* alloc_mem() {
            return !_mp.empty() ? ((void*)_mp.pop_back()) : malloc(sizeof(_Mem));
        }

        void dealloc_mem(void* p) {
            _mp.size() < 4095 ? _mp.push_back(p) : free(p);
        }

        void* alloc(uint32 n);
        void dealloc(void* p);

      private:
        Array _mp;    // for header (_Mem)
        Array _ks[3]; // for key and string
    };

    struct MemberItem;

    class iterator {
      public:
        typedef const void* T;

        iterator(T* p) : _p(p) {}

        MemberItem* operator->() const {
            return (MemberItem*) _p;
        }

        MemberItem& operator*() const {
            return *(MemberItem*) _p;
        }

        bool operator==(const iterator& i) const {
            return _p == i._p;
        }

        bool operator!=(const iterator& i) const {
            return _p != i._p;
        }

        iterator& operator++() {
            _p += 2;
            return *this;
        }

        iterator operator++(int) {
            iterator r(_p);
            _p += 2;
            return r;
        }

      private:
        T* _p;
    };

    Value() : _mem(0) {}

    ~Value() {
        this->reset();
    }

    Value(const Value& v) {
        _mem = v._mem;
        if (_mem) this->_Ref();
    }

    Value(Value&& v) noexcept : _mem(v._mem) {
        v._mem = 0;
    }

    Value& operator=(const Value& v) {
        if (&v != this) {
            if (_mem) this->_UnRef();
            _mem = v._mem;
            if (_mem) this->_Ref();
        }
        return *this;
    }

    Value& operator=(Value&& v) noexcept {
        if (&v != this) {
            if (_mem) this->_UnRef();
            _mem = v._mem;
            v._mem = 0;
        }
        return *this;
    }

    Value(bool v) {
        _mem = new (Jalloc::instance()->alloc_mem()) _Mem(kBool);
        _mem->b = v;
    }

    Value(int64 v) {
        _mem = new (Jalloc::instance()->alloc_mem()) _Mem(kInt);
        _mem->i = v;
    }

    Value(int32 v)  : Value((int64)v) {}
    Value(uint32 v) : Value((int64)v) {}
    Value(uint64 v) : Value((int64)v) {}

    Value(double v) {
        _mem = new (Jalloc::instance()->alloc_mem()) _Mem(kDouble);
        _mem->d = v;
    }

    // string type:
    //   |        header(8)        | body |
    //   | kind(1) | 3 | length(4) | body |
    //
    // _mem->s and _mem->l point to the beginning of the body.
    // We can simply use _mem->l[-1] to get the length.
    Value(const void* s, size_t n) {
        _mem = new (Jalloc::instance()->alloc_mem()) _Mem(kString);
        _mem->s = (char*) Jalloc::instance()->alloc((uint32)n + 1);
        memcpy(_mem->s, s, n);
        _mem->s[n] = '\0';
        _mem->l[-1] = (uint32)n;
    }

    Value(const char* s)        : Value(s, strlen(s)) {}
    Value(const fastring& s)    : Value(s.data(), s.size()) {}
    Value(const std::string& s) : Value(s.data(), s.size()) {}

    Value(JArray) {
        _mem = new (Jalloc::instance()->alloc_mem()) _Mem(kArray);
        new (&_mem->a) Array(7);
    }

    Value(JObject) {
        _mem = new (Jalloc::instance()->alloc_mem()) _Mem(kObject);
        new (&_mem->a) Array(14);        
    }

    bool is_null() const {
        return _mem == 0;
    }

    bool is_bool() const {
        return _mem && (_mem->type & kBool);
    }

    bool is_int() const {
        return _mem && (_mem->type & kInt);
    }

    bool is_double() const {
        return _mem && (_mem->type & kDouble);
    }

    bool is_string() const {
        return _mem && (_mem->type & kString);
    }

    bool is_array() const {
        return _mem && (_mem->type & kArray);
    }

    bool is_object() const {
        return _mem && (_mem->type & kObject);
    }

    bool get_bool() const {
        assert(this->is_bool());
        return _mem->b;
    }

    int64 get_int64() const {
        assert(this->is_int());
        return _mem->i;
    }

    uint64 get_uint64() const {
        return (uint64) this->get_int64();
    }

    int32 get_int32() const {
        return (int32) this->get_int64();
    }

    uint32 get_uint32() const {
        return (uint32) this->get_int64();
    }

    int get_int() const {
        return (int) this->get_int64();
    }

    double get_double() const {
        assert(this->is_double());
        return _mem->d;
    }

    const char* get_string() const {
        assert(this->is_string());
        return _mem->s;
    }

    // push_back() must be operated on null or array types.
    void push_back(Value&& v) {
        if (_mem) assert(_mem->type & kArray);
        else new (&_mem) Value(Value::JArray());
        _mem->a.push_back(v._mem);
        v._mem = 0;
    }

    void push_back(const Value& v) {
        if (_mem) assert(_mem->type & kArray);
        else new (&_mem) Value(Value::JArray());
        _mem->a.push_back(v._mem);
        if (v._mem) v._Ref();
    }

    Value& operator[](uint32 i) const {
        assert(this->is_array());
        return *(Value*) &_mem->a[i];
    }

    Value& operator[](int i) const {
        return this->operator[]((uint32)i);
    }

    // for array and object, return number of the elements.
    // for string, return the length
    // for other types, return sizeof(type)
    uint32 size() const {
        if (_mem == 0) return 0;
        if (_mem->type & kArray) return _mem->a.size();
        if (_mem->type & kObject) return _mem->a.size() >> 1;
        if (_mem->type & kString) return _mem->l[-1];
        if (_mem->type & kInt) return 8;
        if (_mem->type & kBool) return 1;
        if (_mem->type & kDouble) return 8;
        return 0;
    }

    bool empty() const {
        return this->size() == 0;
    }

    // add_member() must be operated on null or object types.
    // add_member(key, val) is faster than obj[key] = val
    void add_member(Key key, Value&& val) {
        if (_mem) assert(_mem->type & kObject);
        else new (&_mem) Value(Value::JObject());
        _mem->a.push_back(_Alloc_key(key));
        _mem->a.push_back(val._mem);
        val._mem = 0;
    }

    void add_member(Key key, const Value& val) {
        if (_mem) assert(_mem->type & kObject);
        else new (&_mem) Value(Value::JObject());
        _mem->a.push_back(_Alloc_key(key));
        _mem->a.push_back(val._mem);
        if (val._mem) val._Ref();
    }

    Value& operator[](Key key) const;

    Value find(Key key) const;

    bool has_member(Key key) const;

    // iterator must be operated on null or object types.
    iterator begin() const {
        assert(_mem == 0 || (_mem->type & kObject));
        return iterator(_mem ? &_mem->a[0] : 0);
    }

    iterator end() const {
        return iterator(_mem ? &_mem->a[_mem->a.size()] : 0);
    }

    // json to string
    fastring str() const {
        fastring s(256);
        this->_Json2str(*(fastream*)&s);
        return std::move(s);
    }

    // write json string to fastream
    void str(fastream& fs) const {
        this->_Json2str(fs);
    }

    // json to debug string
    fastring dbg() const {
        fastring s(256);
        this->_Json2str(*(fastream*)&s, true);
        return std::move(s);
    }

    // write json debug string to fastream
    void dbg(fastream& fs) const {
        this->_Json2str(fs, true);
    }

    // json to pretty string
    fastring pretty(int indent=4) const {
        fastring s(256);
        this->_Json2pretty(*(fastream*)&s, indent, indent);
        return std::move(s);
    }

    bool parse_from(const char* s, size_t n);

    bool parse_from(const char* s) {
        return this->parse_from(s, strlen(s));
    }

    bool parse_from(const fastring& s) {
        return this->parse_from(s.data(), s.size());
    }

    bool parse_from(const std::string& s) {
        return this->parse_from(s.data(), s.size());
    }

    void swap(Value& v) noexcept {
        _Mem* mem = _mem;
        _mem = v._mem;
        v._mem = mem;
    }

    void swap(Value&& v) noexcept {
        v.swap(*this);
    }

    void reset() {
        if (!_mem) return;
        this->_UnRef();
        _mem = 0;
    }

  private:
    void _Ref() const {
        atomic_inc(&_mem->refn);
    }

    void _UnRef();

    void* _Alloc_key(Key key) const {
        size_t len = strlen(key);
        void* s = Jalloc::instance()->alloc((uint32)len + 1);
        memcpy(s, key, len + 1);
        return s;
    }

    void _Json2str(fastream& fs, bool debug=false) const;
    void _Json2pretty(fastream& fs, int indent, int n) const;

    friend const char* parse_object(const char* b, const char* e, fastream& s, Value* r);
    friend const char* parse_array(const char* b, const char* e, fastream& s, Value* r);

  private:
    struct _Mem {
        _Mem(Type t) : type(t), refn(1) {}
        uint32 type;
        uint32 refn;
        union {
            bool b;
            int64 i;
            double d;
            Array a;     // for array and object
            char* s;     // for string
            uint32* l;   // for length of string
        };
    }; // 16 bytes

    _Mem* _mem;
};

struct Value::MemberItem {
    const char* const key;
    Value value;
};

// return an empty array
inline Value array() {
    return Value(Value::JArray());
}

// return an empty object
inline Value object() {
    return Value(Value::JObject());
}

inline Value parse(const char* s, size_t n) {
    Value v;
    if (v.parse_from(s, n)) return v;
    return Value();
}

inline Value parse(const char* s) {
    return parse(s, strlen(s));
}

inline Value parse(const fastring& s) {
    return parse(s.data(), s.size());
}

inline Value parse(const std::string& s) {
    return parse(s.data(), s.size());
}

} // namespace json

typedef json::Value Json;

inline fastream& operator<<(fastream& fs, const Json& v) {
    v.dbg(fs);
    return fs;
}
