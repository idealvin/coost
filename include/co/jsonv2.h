#if 0
#include "fastream.h"
#include <vector>

namespace jsonv2 {
namespace xx {

class Mem {
  public:
    Mem() : Mem(256) {}

    // @n: initial bytes to be allocated, will be made 8-byte aligned
    explicit Mem(size_t n) {
        const uint32 cap = (uint32)((n >> 3) + !!(n & 7));
        _mem = (_Mem*) malloc(sizeof(_Mem) + 8 * cap);
        _mem->cap = cap;
        _mem->size = 0;
    }

    ~Mem() {
        free(_mem);
    }

    // @n: number of 8-byte to be allocated
    uint32 alloc(uint32 n) {
        uint32 index = _mem->size;
        if ((_mem->size += n) > _mem->cap) {
            _mem->cap += (_mem->cap >> 1);
            _mem = (_Mem*) realloc(_mem, sizeof(_Mem) + 8 * _mem->cap);
            assert(_mem);
        }
        return index;
    }

    // alloc 16 bytes for the json header
    uint32 alloc_header() {
        return this->alloc(2);
    }

    // @n: bytes to be allocated for the string
    uint32 alloc_string(size_t n) {
        const uint32 e = (uint32)((n >> 3) + !!(n & 7));
        return this->alloc(e);
    }

    // alloc 64 bytes for each piece of array (14 elements)
    uint32 alloc_array() {
        return this->alloc(8);
    }

    // alloc 128 bytes for each piece of object (15 elements) 
    uint32 alloc_object() {
        return this->alloc(16);
    }

    void* at(uint32 index) {
        return _mem->p + index;
    }

    void clear() {
        _mem->size = 0;
    }

  private:
    struct _Mem {
        uint32 cap;
        uint32 size;
        uint64 p[];
    }; // 8 bytes

    _Mem* _mem;
};

class Jalloc {
  public:
    Jalloc() = default;
    ~Jalloc() = default;

    Mem* alloc_mem() {
        if (!_m.empty()) {
            Mem* m = _m.back();
            _m.pop_back();
            return m;
        }
        return new Mem();
    }

    void dealloc_mem(Mem* m) {
        m->clear();
        _m.push_back(m);
    }

  private:
    std::vector<Mem*> _m;
};

inline Jalloc* jalloc() {
    static __thread Jalloc* alloc = 0;
    return alloc ? alloc : (alloc = new Jalloc);
}

// 
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
    struct _Mem {
        uint32 size;
        uint32 next;
        uint32 p[];
    };
    _Mem* _mem;
};
} // xx

class Root;

class Value {
  public:
    Value();
    ~Value();

  private:
    Root* _root;
    uint32 _index;
};


class Root {
  public:
    enum Type {
        kNull = 0,
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

    Root() : _mem(0) {}

    Root(JArray) : _mem(xx::jalloc()->alloc_mem()) {
        const uint32 index = _mem->alloc_header();
        assert(index == 0);
        _Val* v = new (_mem->at(0)) _Val(kArray);
        v->p = 0;
    }

    Root(JObject) : _mem(xx::jalloc()->alloc_mem()) {
        const uint32 index = _mem->alloc(2);
        assert(index == 0);
        _Val* v = new (_mem->at(0)) _Val(kObject);
        v->p = 0;
    }

    ~Root() {
        if (_mem) xx::jalloc()->dealloc_mem(_mem);
    }

    void add_member(Key key, bool x) {
        uint32 index = _mem->alloc_header();
        _Val* v = new (_mem->at(index)) _Val(kBool);
        v->b = x;

    }

  private:
    struct _Val {
        _Val(Type t) : type(t) {}
        uint32 type;
        uint32 index;
        union {
            bool b;   // for bool
            int64 i;  // for int
            double d; // for double
            uint32 p; // for string, array, object
        };
    }; // 16 bytes

    _Val* _val() {
        return (_Val*)_mem->at(0);
    }

    xx::Mem* _mem;
};

} // jsonv2

#endif