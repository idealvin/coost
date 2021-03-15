#pragma once

#include "atomic.h"
#include "fastream.h"
#include <vector>

#ifdef _MSC_VER
#pragma warning (disable:4200)
#pragma warning (disable:4624)
#endif

namespace json {
namespace xx {

// B: block size
// P: page size, default is 4096
template <int B, int P=4096>
class MemPool {
  public:
    MemPool() : _page((char*)malloc(P)), _index(0) {
        _pages.reserve(128);
        _pages.push_back(_page);
    }

    ~MemPool() {
        for (size_t i = 0; i < _pages.size(); ++i) {
            free(_pages[i]);
        }
    }

    void* alloc() {
        if (_index < P / B) return _page + (_index++) * B;
        _index = 1;
        _page = (char*) malloc(P);
        _pages.push_back(_page);
        return _page;
    }

  private:
    std::vector<char*> _pages;
    char* _page;
    int _index;
};

struct Cache {
    void* pop() {
        if (!_cache.empty()) {
            void* p = _cache.back();
            _cache.pop_back();
            return p;
        }
        return 0;
    }

    void push(void* p) {
        _cache.push_back(p);
    }

    std::vector<void*> _cache;
};

class Jalloc {
    public:
    Jalloc() : _fs(256) {
    }
    ~Jalloc() = default;

    void* alloc_mem() {
        void* p = _c16.pop();
        return p ? p : _mp16.alloc();
    }

    void dealloc_mem(void* p) {
        _c16.push(p);
    }

    // | 4 | 4 | n |
    void* alloc(uint32 n) {
        uint32* p;
        if (n <= 32 - 8) {
            if ((p = (uint32*)_c32.pop())) return p;
            *(p = (uint32*)_mp32.alloc()) = 32 - 8;
        } else if (n <= 64 - 8) {
            if ((p = (uint32*)_c64.pop())) return p;
            *(p = (uint32*)_mp64.alloc()) = 64 - 8;
        } else if (n <= 128 - 8) {
            if ((p = (uint32*)_c128.pop())) return p;
            *(p = (uint32*)_mp128.alloc()) = 128 - 8;
        } else if (n <= 256 - 8) {
            if ((p = (uint32*)_c256.pop())) return p;
            *(p = (uint32*)_mp256.alloc()) = 256 - 8;
        } else if (n <= 512 - 8) {
            if ((p = (uint32*)_c512.pop())) return p;
            *(p = (uint32*)_mp512.alloc()) = 512 - 8;
        } else if (n <= 1024 - 8) {
            if ((p = (uint32*)_c1024.pop())) return p;
            *(p = (uint32*)_mp1024.alloc()) = 1024 - 8;
        } else {
            *(p = (uint32*)malloc(n + 8)) = n;
        }
        return p + 2;
    }

    void dealloc(void* p) {
        uint32* const s = (uint32*)p - 2;
        const uint32 n = *s;
        if (n <= 32 - 8) {
            _c32.push(p);
        } else if (n <= 64 - 8) {
            _c64.push(p);
        } else if (n <= 128 - 8) {
            _c128.push(p);
        } else if (n <= 256 - 8) {
            _c256.push(p);
        } else if (n <= 512 - 8) {
            _c512.push(p);
        } else if (n <= 1024 - 8) {
            _c1024.push(p);
        } else {
            free(s);
        }
    }

    // | 4 | 4 |  p  |
    // | 4 | 4 |  q  |
    void* realloc(void* p, uint32 n) {
        uint32* l = (uint32*)p;
        uint32* q = (uint32*)this->alloc(n);
        memcpy(q - 1, l - 1, l[-2] + 4);
        this->dealloc(p);
        return q;
    }

    char* alloc_string(uint32 n) {
        return (char*) this->alloc(n);
    }

    void dealloc_string(void* p) {
        return this->dealloc(p);
    }

    fastream& alloc_stream() {
        _fs.clear();
        return _fs;
    }

  private:
    MemPool<16> _mp16;
    MemPool<32> _mp32;
    MemPool<64> _mp64;
    MemPool<128> _mp128;
    MemPool<256> _mp256;
    MemPool<512> _mp512;
    MemPool<1024> _mp1024;
    Cache _c16;
    Cache _c32;
    Cache _c64;
    Cache _c128;
    Cache _c256;
    Cache _c512;
    Cache _c1024;
    fastream _fs; // for parsing string
};

inline Jalloc* jalloc() {
    static __thread Jalloc* kJalloc = 0;
    return kJalloc ? kJalloc : (kJalloc = new Jalloc);
}

class Array {
  public:
    typedef void* T;
    Array() : _p(0) {}

    explicit Array(uint32 cap) {
        _p = (T*) xx::jalloc()->alloc(sizeof(T) * cap);
        _l[-1] = 0;
    }

    ~Array() {
        if (_p) xx::jalloc()->dealloc(_p);
    }

    uint32 size() const {
        return _l ? _l[-1]: 0;
    }

    bool empty() const {
        return this->size() == 0;
    }

    T& front() const {
        return _p[0];
    }

    T& back() const {
        return _p[_l[-1] - 1];
    }

    T& operator[](uint32 i) const {
        return _p[i];
    }

    void push_back(T v) {
        if ((_l[-1] + 1) * sizeof(T) >= _l[-2]) {
            _p = (T*) xx::jalloc()->realloc(_p, _l[-2] << 1);
            assert(_p);
        }
        _p[_l[-1]++] = v;
    }

    T pop_back() {
        return _p[--_l[-1]];
    }

  private:
    union {
        T* _p;
        uint32* _l;
    };
};
} // xx

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


    struct MemberItem;

    class iterator {
      public:
        typedef void* T;

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

    Value(): _p(0) {}

    ~Value() {
        this->reset();
    }

    Value(const Value& v) : _p(v._p) {
        if (!this->is_null()) this->_Ref();
    }

    Value(Value&& v) noexcept : _p(v._p) {
        v._p = 0;
    }

    Value& operator=(const Value& v) {
        if (&v != this) {
            if (!this->is_null()) this->_UnRef();
            _p = v._p;
            if (!this->is_null()) this->_Ref();
        }
        return *this;
    }

    Value& operator=(Value&& v) noexcept {
        if (&v != this) {
            if (!this->is_null()) this->_UnRef();
            _p = v._p;
            v._p = 0;
        }
        return *this;
    }

    Value(bool v) {
        _p = new (xx::jalloc()->alloc_mem()) _Mem(kBool);
        _p->b = v;
    }

    Value(int64 v) {
        _p = new (xx::jalloc()->alloc_mem()) _Mem(kInt);
        _p->i = v;
    }

    Value(int32 v)  : Value((int64)v) {}
    Value(uint32 v) : Value((int64)v) {}
    Value(uint64 v) : Value((int64)v) {}

    Value(double v) {
        _p = new (xx::jalloc()->alloc_mem()) _Mem(kDouble);
        _p->d = v;
    }

    // string type:
    //   |        header(8)        | body |
    //   | reserved(4) | length(4) | body |
    //
    // _mem->s and _mem->l point to the beginning of the body.
    // We can simply use _mem->l[-1] to get the length.
    Value(const void* s, size_t n) {
        _p = new (xx::jalloc()->alloc_mem()) _Mem(kString);
        _p->s = xx::jalloc()->alloc_string((uint32)n + 1);
        _p->s[n] = '\0';
        _p->l[-1] = (uint32)n;
        memcpy(_p->s, s, n);
    }

    Value(const char* s)        : Value(s, strlen(s)) {}
    Value(const fastring& s)    : Value(s.data(), s.size()) {}
    Value(const std::string& s) : Value(s.data(), s.size()) {}

    Value(JArray) {
        _p = new (xx::jalloc()->alloc_mem()) _Mem(kArray);
        new (&_p->a) xx::Array(7);
    }

    Value(JObject) {
        _p = new (xx::jalloc()->alloc_mem()) _Mem(kObject);
        new (&_p->o) xx::Array(14);
    }

    bool is_null() const {
        return _p == 0;
    }

    bool is_bool() const {
        return _p && _p->type == kBool;
    }

    bool is_int() const {
        return _p && _p->type == kInt;
    }

    bool is_double() const {
        return _p && _p->type == kDouble;
    }

    bool is_string() const {
        return _p && _p->type == kString;
    }

    bool is_array() const {
        return _p && _p->type == kArray;
    }

    bool is_object() const {
        return _p && _p->type == kObject;
    }

    bool get_bool() const {
        assert(this->is_bool());
        return _p->b;
    }

    int64 get_int64() const {
        assert(this->is_int());
        return _p->i;
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
        return _p->d;
    }

    const char* get_string() const {
        assert(this->is_string());
        return _p->s;
    }

    // push_back() must be operated on null or array types.
    void push_back(Value&& v) {
        if (!this->is_null()) assert(_p->type == kArray);
        else new (this) Value(JArray());
        _p->a.push_back(v._p);
        v._p = 0;
    }

    void push_back(const Value& v) {
        if (!this->is_null()) assert(_p->type == kArray);
        else new (this) Value(JArray());
        _p->a.push_back(v._p);
        if (&v != this && !v.is_null()) v._Ref();
    }

    const Value& operator[](uint32 i) const {
        assert(this->is_array());
        return *(Value*) &_p->a[i];
    }

    Value& operator[](uint32 i) {
        assert(this->is_array());
        return *(Value*) &_p->a[i];
    }

    const Value& operator[](int i) const {
        return this->operator[]((uint32)i);
    }

    Value& operator[](int i) {
        return this->operator[]((uint32)i);
    }

    // for array and object, return number of the elements.
    // for string, return the length
    // for other types, return sizeof(type)
    uint32 size() const {
        if (this->is_null()) return 0;
        if (_p->type == kArray) return _p->a.size();
        if (_p->type == kObject) return _p->o.size() >> 1;
        if (_p->type == kString) return _p->l[-1];
        if (_p->type == kInt) return 8;
        if (_p->type == kBool) return 1;
        if (_p->type == kDouble) return 8;
        return 0;
    }

    bool empty() const {
        return this->size() == 0;
    }

    // add_member() must be operated on null or object types.
    // add_member(key, val) is faster than obj[key] = val
    void add_member(Key key, Value&& val) {
        if (!this->is_null()) assert(_p->type == kObject);
        else new (this) Value(JObject());
        _p->o.push_back(_Alloc_key(key));
        _p->o.push_back(val._p);
        val._p = 0;
    }

    void add_member(Key key, const Value& val) {
        if (!this->is_null()) assert(_p->type == kObject);
        else new (this) Value(JObject());
        _p->o.push_back(_Alloc_key(key));
        _p->o.push_back(val._p);
        if (&val != this && !val.is_null()) val._Ref();
    }

    Value& operator[](Key key);
    const Value& operator[](Key key) const;

    Value find(Key key) const {
        return this->operator[](key);
    }

    bool has_member(Key key) const;

    // iterator must be operated on null or object types.
    iterator begin() const {
        if (!this->is_null()) {
            assert(_p->type == kObject);
            return iterator(&_p->o[0]);
        }
        return iterator(0);
    }

    iterator end() const {
        if (!this->is_null()) {
            return iterator(&_p->o[_p->o.size()]);
        }
        return iterator(0);
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
        _Mem* const p = _p;
        _p = v._p;
        v._p = p;
    }

    void swap(Value&& v) noexcept {
        v.swap(*this);
    }

    void reset() {
        if (!this->is_null()) {
            this->_UnRef();
            _p = 0;
        }
    }

  private:
    void _Ref() const {
        atomic_inc(&_p->refn);
    }

    void _UnRef();

    void* _Alloc_key(Key key) const {
        uint32 len = (uint32) strlen(key);
        uint32* s = (uint32*) xx::jalloc()->alloc_string(len + 1);
        memcpy(s, key, len + 1);
        return s;
    }

    void _Json2str(fastream& fs, bool debug=false) const;
    void _Json2pretty(fastream& fs, int indent, int n) const;

    struct _Mem {
        _Mem(Type t) : type(t), refn(1) {}
        uint32 type;
        uint32 refn;
        union {
            bool b;      // for bool
            int64 i;     // for int
            double d;    // for double
            xx::Array a; // for array
            xx::Array o; // for object
            char* s;     // for string
            uint32* l;   // for strlen
        };
    }; // 16 bytes

  private:
    _Mem* _p;
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
    if (v.parse_from(s, n)) return std::move(v);
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
