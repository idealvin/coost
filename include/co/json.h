#pragma once

#include "array.h"
#include "atomic.h"
#include "fastream.h"

namespace json {

class Value {
  public:
    enum {
        kBool = 1,
        kInt = 2,
        kDouble = 4,
        kString = 8,
        kArray = 16,
        kObject = 32,
    };

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
            _mp.size() < 8 * 1024 ? _mp.push_back(p) : free(p);
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

        Key key() const {
            return (Key) _p[0];
        }

        Value& value() const {
            return *(Value*) &_p[1];
        }

        bool operator==(const iterator& i) const {
            return _p == i._p;
        }

        bool operator!=(const iterator& i) const {
            return _p != i._p;
        }

        // ++it
        iterator& operator++() {
            _p += 2;
            return *this;
        }

        // it++
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
        if (_mem) this->_UnRef();
        _mem = v._mem;
        v._mem = 0;
        return *this;
    }

    Value(bool v) {
        _mem = (_Mem*) Jalloc::instance()->alloc_mem();
        _mem->type = kBool;
        _mem->refn = 1;
        _mem->b = v;
    }

    Value(int64 v) {
        _mem = (_Mem*) Jalloc::instance()->alloc_mem();
        _mem->type = kInt;
        _mem->refn = 1;
        _mem->i = v;
    }

    Value(int32 v) : Value((int64)v) {}
    Value(uint32 v) : Value((int64)v) {}
    Value(uint64 v) : Value((int64)v) {}

    Value(double v) {
        _mem = (_Mem*) Jalloc::instance()->alloc_mem();
        _mem->type = kDouble;
        _mem->refn = 1;
        _mem->d = v;
    }

    Value(const char* v) {
        this->_Init_string(v, strlen(v));
    }

    Value(const fastring& v) {
        this->_Init_string(v.data(), v.size());
    }

    Value(const std::string& v) {
        this->_Init_string(v.data(), v.size());
    }

    Value(const void* data, size_t size) {
        this->_Init_string(data, size);
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

    // null terminated string
    const char* get_string() const {
        assert(this->is_string());
        return _mem->s;
    }

    void set_array() {
        if (_mem) {
            if (_mem->type & kArray) return;
            this->_UnRef();
        }
        this->_Init_array();
    }

    void set_object() {
        if (_mem) {
            if (_mem->type & kObject) return;
            this->_UnRef();
        }
        this->_Init_object();
    }

    // for array type
    void push_back(Value&& v) {
        this->_Push_back(v._mem);
        v._mem = 0;
    }

    void push_back(const Value& v) {
        this->_Push_back(v._mem);
        if (v._mem) v._Ref();
    }

    Value& operator[](uint32 i) const {
        assert(this->is_array());
        return *(Value*) &_Array()[i];
    }

    Value& operator[](int i) const {
        return this->operator[]((uint32)i);
    }

    // for array and object, return number of the elements.
    // for string, return the length
    // for other types, return sizeof(type)
    uint32 size() const {
        if (_mem == 0) return 0;
        if (_mem->type & kObject) return _Array().size() >> 1;
        if (_mem->type & kArray) return _Array().size();
        if (_mem->type & kString) return _mem->l[-1];
        if (_mem->type & kInt) return 8;
        if (_mem->type & kBool) return 1;
        if (_mem->type & kDouble) return 8;
        return 0;
    }

    bool empty() const {
        return this->size() == 0;
    }

    // prefer to use find() than has_member() and operator[].
    // return null if the key not found.
    //   Json x = obj.find(key);
    //   if (!x.is_null()) do_something();
    Value find(Key key) const;

    bool has_member(Key key) const;

    // add_member(key, val) is faster than obj[key] = val
    void add_member(Key key, Value&& val) {
        this->_Add_member(key, val._mem);
        val._mem = 0;
    }

    void add_member(Key key, const Value& val) {
        this->_Add_member(key, val._mem);
        if (val._mem) val._Ref();
    }

    iterator begin() const {
        return iterator(_mem ? &_Array()[0] : 0);
    }

    iterator end() const {
        return iterator(_mem ? &_Array()[_Array().size()] : 0);
    }

    Value& operator[](Key key) const;

    // stringify
    fastring str() const {
        fastring s(256);
        this->_Json2str(*(fastream*)&s);
        return s;
    }

    // append json string to @fs
    void str(fastream& fs) const {
        this->_Json2str(fs);
    }

    // json to debug string
    fastring dbg() const {
        fastring s(256);
        this->_Json2dbg(*(fastream*)&s);
        return s;
    }

    void dbg(fastream& fs) const {
        this->_Json2dbg(fs);
    }

    // convert json to pretty string
    fastring pretty(int indent = 4) const {
        fastring s(256);
        this->_Json2pretty(indent, indent, *(fastream*)&s);
        return s;
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

    void _Init_string(const void* data, size_t size);

    Array& _Array() const {
        return *(Array*) &_mem->p;
    }

    void _Init_array() {
        _mem = (_Mem*) Jalloc::instance()->alloc_mem();
        _mem->type = kArray;
        _mem->refn = 1;
        new (&_mem->p) Array(8);
    }

    void _Init_object() {
        _mem = (_Mem*) Jalloc::instance()->alloc_mem();
        _mem->type = kObject;
        _mem->refn = 1;
        new (&_mem->p) Array(16);
    }

    void _Assert_array() {
        if (_mem) {
            assert(_mem->type & kArray);
        } else {
            this->_Init_array();
        }
    }

    void _Assert_object() {
        if (_mem) {
            assert(_mem->type & kObject);
        } else {
            this->_Init_object();
        }
    }

    void _Push_back(void* v) {
        _Assert_array();
        _Array().push_back(v);
    }

    void* _Alloc_key(Key key) const {
        size_t len = strlen(key);
        void* s = Jalloc::instance()->alloc((uint32)len + 1);
        memcpy(s, key, len + 1);
        return s;
    }

    void _Add_member(Key key, void* val) {
        _Assert_object();
        _Array().push_back(_Alloc_key(key));
        _Array().push_back(val);
    }

    void _Json2str(fastream& fs) const;
    void _Json2dbg(fastream& fs) const;
    void _Json2pretty(int base_indent, int current_indent, fastream& fs) const;

    friend const char* parse_json (const char*, const char*, Value*);
    friend const char* parse_array(const char*, const char*, Value*);

  private:
    struct _Mem {
        int32 type;
        int32 refn;
        union {
            bool b;
            int64 i;
            double d;
            void* p;     // for array and object
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
    Value v;
    v.set_array();
    return v;
}

// return an empty object
inline Value object() {
    Value v;
    v.set_object();
    return v;
}

// parse json from string
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
