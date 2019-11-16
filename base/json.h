#pragma once

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

    typedef const void* T;
    typedef const char* Key;
    struct MemberItem;

    class iterator {
      public:
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

    struct Array {
        Array(int type, uint32 cap) {
            _mem = (_Mem*) malloc(sizeof(_Mem) + sizeof(T) * cap);
            _mem->type = type;
            _mem->refn = 1;
            _mem->cap = cap;
            _mem->size = 0;
        }

        uint32 size() const {
            return _mem->size;
        }

        bool empty() const {
            return this->size() == 0;
        }

        T& back() const {
            return ((T*)(_mem + 1))[_mem->size - 1];
        }

        T& operator[](uint32 i) const {
            return ((T*)(_mem + 1))[i];
        }

        void push_back(T v) {
            if (_mem->size == _mem->cap) {
                _mem = (_Mem*) realloc(_mem, sizeof(_Mem) + sizeof(T) * (_mem->cap << 1));
                assert(_mem);
                _mem->cap <<= 1;
            }
            ((T*) (_mem + 1))[_mem->size++] = v;
        }

        struct _Mem {
            int32 type;
            int32 refn;
            uint32 cap;
            uint32 size;
        }; // 16 bytes

        _Mem* _mem;
    };

    Value() : _mem(0) {}

    ~Value() {
        this->reset();
    }

    Value(const Value& v) {
        _mem = v._mem;
        if (_mem) ++_mem->refn;
    }

    Value(Value&& v) {
        _mem = v._mem;
        v._mem = 0;
    }

    Value& operator=(const Value& v) {
        if (&v != this) {
            if (_mem) this->reset();
            _mem = v._mem;
            if (_mem) ++_mem->refn;
        }
        return *this;
    }

    Value& operator=(Value&& v) {
        if (_mem) this->reset();
        _mem = v._mem;
        v._mem = 0;
        return *this;
    }

    Value(bool v) {
        _mem = (_Mem*) malloc(sizeof(_Mem));
        _mem->refn = 1;
        _mem->type = kBool;
        _mem->b = v;
    }

    Value(int32 v) {
        _mem = (_Mem*) malloc(sizeof(_Mem));
        _mem->refn = 1;
        _mem->type = kInt;
        _mem->i = v;
    }

    Value(int64 v) {
        _mem = (_Mem*) malloc(sizeof(_Mem));
        _mem->refn = 1;
        _mem->type = kInt;
        _mem->i = v;
    }

    Value(double v) {
        _mem = (_Mem*) malloc(sizeof(_Mem));
        _mem->refn = 1;
        _mem->type = kDouble;
        _mem->d = v;
    }

    Value(const char* v) {
        this->_Set_string(v, strlen(v));
    }

    Value(const fastring& v) {
        this->_Set_string(v.data(), v.size());
    }

    Value(const void* data, size_t size) {
        this->_Set_string(data, size);
    }

    bool is_null() const {
        return _mem == 0;
    }

    bool is_bool() const {
        return _mem && (_mem->type == kBool);
    }

    bool is_int() const {
        return _mem && (_mem->type == kInt);
    }

    bool is_double() const {
        return _mem && (_mem->type == kDouble);
    }

    bool is_string() const {
        return _mem && (_mem->type == kString);
    }

    bool is_array() const {
        return _mem && (_mem->type == kArray);
    }

    bool is_object() const {
        return _mem && (_mem->type == kObject);
    }

    bool get_bool() const {
        assert(this->is_bool());
        return _mem->b;
    }

    int64 get_int64() const {
        assert(this->is_int());
        return _mem->i;
    }

    int32 get_int32() const {
        return (int32) this->get_int64();
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
        return (const char*) (_mem + 1);
    }

    void set_object() {
        if (_mem ) {
            if (_mem->type == kObject) return;
            this->reset();
        }
        new (&_mem) Array(kObject, 16);
    }

    void set_array() {
        if (_mem) {
            if (_mem->type == kArray) return;
            this->reset();
        }
        new (&_mem) Array(kArray, 8);
    }

    void __push_back(void* v) {
        this->_Init_array();
        _Array().push_back(v);
    }

    // for array:  a.push_back(v),  a[index],  a.size()
    void push_back(Value&& v) {
        this->__push_back(v._mem);
        v._mem = 0;
    }

    void push_back(const Value& v) {
        this->__push_back(v._mem);
        if (v._mem) ++v._mem->refn;
    }

    Value& operator[](uint32 i) const {
        assert(this->is_array());
        return *(Value*) &_Array()[i];
    }

    // for array and object, return number of the elements.
    // for string, return the length
    // for other types, return sizeof(type)
    uint32 size() const {
        if (_mem == 0) return 0;
        if (_mem->type == kArray) return _Array().size();
        if (_mem->type == kString) return _mem->slen;
        if (_mem->type == kObject) {
            if (_Array().size() == 0) return 0;
            if (_Array()[0]) return _Array().size() >> 1;
            return (_Array().size() >> 1) - 1;
        }
        if (_mem->type == kInt) return 8;
        if (_mem->type == kBool) return 1;
        if (_mem->type == kDouble) return 8;
        return 0;
    }

    bool empty() const {
        return this->size() == 0;
    }

    // find() is more efficient than has_member() and operator[].
    // return null if not found
    //   Json obj;
    //   Json x = obj.find(key);
    //   if (!x.is_null()) do_something();
    //   if (!(x = obj.find(another_key)).is_null()) do_something();
    Value find(Key key) const;

    bool has_member(Key key) const {
        return !this->find(key).is_null();
    }

    void __add_member(Key key, void* val) {
        this->_Init_object();
        _Array().push_back(key);
        _Array().push_back(val);
    }

    // add_member(key, val) is faster than obj[key] = val
    void add_member(Key key, Value&& val) {
        this->__add_member(key, val._mem);
        val._mem = 0;
    }

    void add_member(Key key, const Value& val) {
        this->__add_member(key, val._mem);
        if (val._mem) ++val._mem->refn;
    }

    iterator begin() const {
        if (_Array().empty() || _Array()[0]) return iterator(&_Array()[0]);
        return iterator(&_Array()[2]);
    }

    iterator end() const {
        return iterator(&_Array()[_Array().size()]);
    }

    Value& operator[](Key key) const;

    // json to string
    fastring str(uint32 cap = 256) const {
        magicstream ms(cap);
        this->_Json2str(ms.stream());
        return ms.str();
    }

    // append json string to @fs
    void str(fastream& fs) const {
        this->_Json2str(fs);
    }

    // convert json to pretty string
    fastring pretty(int indent = 4, uint32 cap = 256) const {
        magicstream ms(cap);
        this->_Json2pretty(indent, indent, ms.stream());
        return ms.str();
    }

    bool parse_from(const char* s, size_t n);

    bool parse_from(const char* s) {
        return this->parse_from(s, strlen(s));
    }

    bool parse_from(const fastring& s) {
        return this->parse_from(s.data(), s.size());
    }

    void swap(Value& v) {
        if (&v == this) return;
        _Mem* mem = _mem;
        _mem = v._mem;
        v._mem = mem;
    }

    void swap(Value&& v) {
        v.swap(*this);
    }

    void reset();

  private:
    void _Set_string(const void* data, size_t size) {
        _mem = (_Mem*) malloc(sizeof(_Mem) + size + 1);
        _mem->refn = 1;
        _mem->type = kString;
        _mem->slen = (uint32) size;
        memcpy(_mem + 1, data, size);
        ((char*)(_mem + 1))[size] = '\0';
    }

    Array& _Array() const {
        return *(Array*) &_mem;
    }

    void _Init_array() {
        if (_mem == 0) {
            new (&_mem) Array(kArray, 8);
        } else {
            assert(_mem->type == kArray);
        }
    }

    void _Init_object() {
        if (_mem == 0) {
            new (&_mem) Array(kObject, 16);
        } else {
            assert(_mem->type == kObject);
        }
    }

    void _Json2str(fastream& fs) const;
    void _Json2pretty(int base_indent, int current_indent, fastream& fs) const;

  private:
    struct _Mem {
        int32 type;
        int32 refn;

        union {
            bool b;
            int64 i;
            double d;
            uint32 slen;   // for string, save the length
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

} // namespace json

typedef json::Value Json;

inline fastream& operator<<(fastream& fs, const Json& v) {
    v.str(fs);
    return fs;
}
