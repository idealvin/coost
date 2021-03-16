#if 1
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
        _h = (_Header*) malloc(sizeof(_Header) + 8 * cap);
        _h->cap = cap;
        _h->size = 0;
    }

    ~Mem() {
        free(_h);
    }

    // @n: number of 8-byte to be allocated
    uint32 alloc(uint32 n) {
        uint32 index = _h->size;
        if ((_h->size += n) > _h->cap) {
            _h->cap += (_h->cap >> 1);
            _h = (_Header*) realloc(_h, sizeof(_Header) + 8 * _h->cap);
            assert(_h);
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
        return _h->p + index;
    }

    void clear() {
        _h->size = 0;
    }

  private:
    struct _Header {
        uint32 cap;
        uint32 size;
        uint64 p[];
    }; // 8 bytes

    _Header* _h;
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


// bool: 
//   | type: 4 | value: 4 |
//
// int, double:
//   | type: 4 | 4 | value: 8 |
//
// string:
//   | type: 4 | size: 4 | body |
//
// array, object: 
//   | type: 4 | size: 4 | cap: 4 | next_index: 4 | body |
// array, object may have more than one pieces, @next_index points to the next piece.
//
// extra pieces for array, object:
//   | size: 4 | next_index: 4 | body |
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

    ~Root() {
        if (_mem) xx::jalloc()->dealloc_mem(_mem);
    }

    Root(JArray, uint32 cap=8) : _mem(xx::jalloc()->alloc_mem()) {
        const uint32 index = _mem->alloc(_b8(16 + sizeof(uint32) * cap));
        assert(index == 0);
        _Header* h = _header();
        h->type = kArray;
        h->size = 0;
        h->a[0] = cap;
        h->a[1] = 0; // next index
    }

    Root(JObject, uint32 cap=8) : _mem(xx::jalloc()->alloc_mem()) {
        const uint32 index = _mem->alloc(_b8(16 + 2 * sizeof(uint32) * cap));
        assert(index == 0);
        _Header* h = _header();
        h->type = kObject;
        h->size = 0;
        h->a[0] = cap;
        h->a[1] = 0; // next index
    }

    void add_member(Key key, bool x) {
        if (_mem == 0) new (this) Root(JObject());
        _Header* h = _header();
        assert(h->type == kObject);
        
    }

    void add_array(Key key, uint32 cap=8) {
    }

  private:
    uint32 _b8(uint32 n) {
        return (uint32)((n >> 3) + !!(n & 7));
    }
  private:
    struct _Header {
        _Header(Type t) : type(t) {}
        uint32 type;
        union {
            bool b;
            uint32 size;
        };
        union {
            int64 i;
            double d;
            uint32 a[2]; // for array, object
        };
        uint32 p[];
    }; // 16 bytes

    _Header* _header() {
        return (_Header*)_mem->at(0);
    }

    xx::Mem* _mem;
};

} // jsonv2

#endif