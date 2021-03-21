#pragma once

#include "log.h"
#include "fastream.h"
#include <vector>

namespace json {
namespace xx {

// JBlock is an array of uint64.
// json::Root will be parsed or constructed as a JBlock.
class JBlock {
  public:
    enum { N = 8 }; // block size
    explicit JBlock(size_t cap) {
        if (cap < 31) cap = 31;
        _h = (_Header*) malloc(sizeof(_Header) + cap * N);
        _h->cap = (uint32)cap;
        _h->size = 0;
    }

    ~JBlock() {
        free(_h);
    }

    // alloc n block, return the index
    uint32 alloc(uint32 n) {
        uint32 index = _h->size;
        if ((_h->size += n) > _h->cap) {
            _h->cap += (_h->cap >> 1) + n;
            _h = (_Header*) realloc(_h, sizeof(_Header) + _h->cap * N);
            assert(_h);
        }
        return index;
    }

    void* at(uint32 index) const {
        return _h->p + index;
    }

    void reserve(uint32 n) {
        if (n > _h->cap) {
            _h->cap = n;
            _h = (_Header*) realloc(_h, sizeof(_Header) + n * N);
            assert(_h);
        }
    }

    void clear() {
        _h->size = 0;
    }

    void copy_from(const JBlock& m) {
        this->reserve(m._h->size);
        memcpy(_h, m._h, sizeof(_Header) + m._h->size * N);
    }

  private:
    struct _Header {
        uint32 cap;
        uint32 size;
        uint64 p[];
    };
    _Header* _h;
};

class JAlloc {
  public:
    JAlloc() : _fs(256), _stack(512) {
        _jb.reserve(32);
    }

    ~JAlloc() {
        for (size_t i = 0; i < _jb.size(); ++i) free(_jb[i]);
    }

    void* alloc_jblock(size_t cap=31) {
        void* p;
        if (!_jb.empty()) {
            p = _jb.back();
            _jb.pop_back();
            ((JBlock*)&p)->reserve(cap);
        } else {
            new (&p) JBlock(cap);
        }
        return p;
    }

    void dealloc_jblock(void* p) {
        ((JBlock*)&p)->clear();
        _jb.push_back(p);
    }

    fastream& alloc_stream() {
        _fs.clear();
        return _fs;
    }
    
    fastream& alloc_stack() {
        return _stack;
    }

  private:
    std::vector<void*> _jb;
    fastream _fs;    // for parsing string
    fastream _stack; // for parsing array, object
};

inline JAlloc* jalloc() {
    static __thread JAlloc* alloc = 0;
    return alloc ? alloc : (alloc = new JAlloc);
}

// Queue which holds index of elements for array and object.
// Every array or object owns one or more Queue.
// @next: points to the next Queue.
struct Queue {
    Queue(uint32 n)
        : cap(n), size(0), next(0) {
    }

    Queue(uint32 n, uint32 m)
        : cap(n), size(m), next(0) {
    }

    ~Queue() = default;

    void push(uint32 x) {
        p[size++] = x;
    }

    void push(uint32 x, uint32 y) {
        p[size++] = x;
        p[size++] = y;
    }

    bool full() const {
        return size >= cap;
    }

    uint32 cap;
    uint32 size;
    uint32 next;
    uint32 reserved;
    uint32 p[];
};

} // xx

// root node of the json
class Root;

// sub node of Root.
// A Value must be created from a Root.
class Value {
  public:
    Value() = default;
    ~Value() = default;

    typedef const char* Key;

    bool is_null() const;
    bool is_bool() const;
    bool is_int() const;
    bool is_double() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;

    bool get_bool() const;
    int64 get_int64() const;
    int get_int() const;
    int32 get_int32() const;
    uint32 get_uint32() const;
    uint64 get_uint64() const;
    double get_double() const;
    const char* get_string() const;

    void add_member(Key key, bool x);
    void add_member(Key key, int64 x);
    void add_member(Key key, int x);
    void add_member(Key key, uint32 x);
    void add_member(Key key, uint64 x);
    void add_member(Key key, double x);
    void add_member(Key key, const char* x, size_t n);
    void add_member(Key key, const char* x);
    void add_member(Key key, const std::string& x);
    void add_member(Key key, const fastring& x);
    void add_null(Key key);
    Value add_array(Key key, uint32 cap=0);
    Value add_object(Key key, uint32 cap=0);

    void push_back(bool x);
    void push_back(int64 x);
    void push_back(int x);
    void push_back(uint32 x);
    void push_back(uint64 x);
    void push_back(double x);
    void push_back(const char* x, size_t n);
    void push_back(const char* x);
    void push_back(const std::string& x);
    void push_back(const fastring& x);
    void push_null();
    Value push_array(uint32 cap=0);
    Value push_object(uint32 cap=0);

    Value operator[](uint32 i) const;
    Value operator[](int i) const;
    Value operator[](Key key) const;
    bool has_member(Key key) const;
    uint32 size() const;

    class iterator {
      public:
        iterator(Root* root, uint32 index, uint32 type);

        struct End {};
        static const End& end() {
            static End kEnd;
            return kEnd;
        }

        bool operator!=(const End&) const;
        bool operator==(const End&) const;
        iterator& operator++();
        Value operator*() const;

        Key key() const;
        Value value() const;

      private:
        Root* _root;
        uint32 _index;
        uint32 _pos;
        uint32 _step;
    };

    iterator begin() const;

    const iterator::End& end() const {
        return iterator::end();
    }

    fastring str(uint32 cap=256) const;
    void str(fastream& fs) const;
    fastring dbg(uint32 cap=256) const;
    void dbg(fastream& fs) const;
    fastring pretty(int indent=4) const;

  private:
    Root* _root;
    uint32 _index;

    friend class Root;
    Value(Root* root, uint32 index)
        : _root(root), _index(index) {
    }
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

    friend class Value;
    friend class Parser;
    friend class Value::iterator;
    struct TypeArray {};
    struct TypeObject {};
    typedef const char* Key;
    typedef Value::iterator iterator;

    Root() : _mem(xx::jalloc()->alloc_jblock()) {
        _make_null();
    }

    ~Root() {
        if (_mem) xx::jalloc()->dealloc_jblock(_mem);
    }

    Root(TypeArray) : _mem(xx::jalloc()->alloc_jblock()) {
        _make_array();
    }

    Root(TypeObject) : _mem(xx::jalloc()->alloc_jblock()) {
        _make_object();
    }

    Root(Root&& r) noexcept : _mem(r._mem) {
        r._mem = 0;
    }

    Root(const Root& r) = delete;
    Root& operator=(const Root& r) = delete;

    Root& operator=(Root&& r) {
        if (&r != this) {
            if (_mem) xx::jalloc()->dealloc_jblock(_mem);
            _mem = r._mem;
            r._mem = 0;
        }
        return *this;
    }

    void add_member(Key key, bool x) {
        return this->_add_member(_make_key(key), _make_bool(x), 0);
    }

    void add_member(Key key, int64 x) {
        return this->_add_member(_make_key(key), _make_int(x), 0);
    }

    void add_member(Key key, int x) {
        return this->add_member(key, (int64)x);
    }

    void add_member(Key key, uint32 x) {
        return this->add_member(key, (int64)x);
    }

    void add_member(Key key, uint64 x) {
        return this->add_member(key, (int64)x);
    }

    void add_member(Key key, double x) {
        return this->_add_member(_make_key(key), _make_double(x), 0);
    }

    void add_member(Key key, const char* x, size_t n) {
        return this->_add_member(_make_key(key), _make_string(x, n), 0);
    }

    void add_member(Key key, const char* x) {
        return this->add_member(key, x, strlen(x));
    }

    void add_member(Key key, const std::string& x) {
        return this->add_member(key, x.data(), x.size());
    }

    void add_member(Key key, const fastring& x) {
        return this->add_member(key, x.data(), x.size());
    }

    void add_null(Key key) {
        return this->_add_member(_make_key(key), _make_null(), 0);
    }

    Value add_array(Key key, uint32 cap=0) {
        return this->_add_array(key, cap, 0);
    }

    Value add_object(Key key, uint32 cap=0) {
        return this->_add_object(key, cap, 0);
    }

    void push_back(bool x) {
        return this->_push_back(_make_bool(x), 0);
    }

    void push_back(int64 x) {
        return this->_push_back(_make_int(x), 0);
    }

    void push_back(int x) {
        return this->push_back((int64)x);
    }

    void push_back(uint32 x) {
        return this->push_back((int64)x);
    }

    void push_back(uint64 x) {
        return this->push_back((int64)x);
    }

    void push_back(double x) {
        return this->_push_back(_make_double(x), 0);
    }

    void push_back(const char* x, size_t n) {
        return this->_push_back(_make_string(x, n), 0);
    }

    void push_back(const char* x) {
        return this->push_back(x, strlen(x));
    }

    void push_back(const std::string& x) {
        return this->push_back(x.data(), x.size());
    }

    void push_back(const fastring& x) {
        return this->push_back(x.data(), x.size());
    }

    void push_null() {
        return this->_push_back(_make_null(), 0);
    }

    Value push_array(uint32 cap=0) {
        return this->_push_array(cap, 0);
    }

    Value push_object(uint32 cap=0) {
        return this->_push_object(cap, 0);
    }

    bool is_null() const {
        return this->_is_null(0);
    }

    bool is_bool() const {
        return this->_is_bool(0);
    }

    bool is_int() const {
        return this->_is_int(0);
    }

    bool is_double() const {
        return this->_is_double(0);
    }

    bool is_string() const {
        return this->_is_string(0);
    }

    bool is_array() const {
        return this->_is_array(0);
    }

    bool is_object() const {
        return this->_is_object(0);
    }

    bool get_bool() const {
        return this->_get_bool(0);
    }

    int64 get_int64() const {
        return this->_get_int64(0);
    }

    int get_int() const {
        return (int)this->get_int64();
    }

    int32 get_int32() const {
        return (int32)this->get_int64();
    }

    uint32 get_uint32() const {
        return (uint32)this->get_int64();
    }

    uint64 get_uint64() const {
        return (uint64)this->get_int64();
    }

    double get_double() const {
        return this->_get_double(0);
    }

    const char* get_string() const {
        return this->_get_string(0);
    }

    fastring str(uint32 cap=256) const {
        fastring s(cap);
        this->_Json2str(*(fastream*)&s, false, 0);
        return std::move(s);
    }

    // write json string to fastream
    void str(fastream& fs) const {
        this->_Json2str(fs, false, 0);
    }

    // json to debug string
    fastring dbg(uint32 cap=256) const {
        fastring s(cap);
        this->_Json2str(*(fastream*)&s, true, 0);
        return std::move(s);
    }

    // write json debug string to fastream
    void dbg(fastream& fs) const {
        this->_Json2str(fs, true, 0);
    }

    // json to pretty string
    fastring pretty(int indent=4) const {
        fastring s(256);
        this->_Json2pretty(*(fastream*)&s, indent, indent, 0);
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

    iterator begin() const {
        return this->_begin(0);
    }

    const iterator::End& end() const {
        return iterator::end();
    }

    Value operator[](uint32 i) const {
        return this->_at(i, 0);
    }

    Value operator[](int i) const {
        return this->operator[]((uint32)i);
    }

    Value operator[](Key key) const {
        return this->_at(key, 0);
    }

    bool has_member(Key key) const {
        return this->_has_member(key, 0);
    }

    uint32 size() const {
        return this->_size(0);
    }

    void reset() {
        _jb.clear();
        _make_null();
    }

  private:
    void _Json2str(fastream& fs, bool debug, uint32 index) const;
    void _Json2pretty(fastream& fs, int indent, int n, uint32 index) const;

    Value _at(uint32 i, uint32 index) const;
    Value _at(Key key, uint32 index) const;
    bool _has_member(Key key, uint32 index) const;
    uint32 _size(uint32 index) const;

    uint32 _b8(uint32 n) {
        return (uint32)((n >> 3) + !!(n & 7));
    }

    iterator _begin(uint32 index) const {
        assert(_mem);
        _Header* h = (_Header*) _jb.at(index);
        assert(h->type == kObject || h->type == kArray);
        return iterator((Root*)this, h->index, h->type);
    }

    uint32 _alloc_header() {
        return _jb.alloc(2); // 16 bytes
    }

    uint32 _make_null() {
        const uint32 index = _alloc_header();
        new (_jb.at(index)) _Header(kNull);
        return index;
    }

    uint32 _make_bool(bool x) {
        const uint32 index = _alloc_header();
        (new (_jb.at(index)) _Header(kBool))->b = x;
        return index;
    }

    uint32 _make_int(int64 x) {
        const uint32 index = _alloc_header();
        (new (_jb.at(index)) _Header(kInt))->i = x;
        return index;
    }

    uint32 _make_double(double x) {
        const uint32 index = _alloc_header();
        (new (_jb.at(index)) _Header(kDouble))->d = x;
        return index;
    }

    uint32 _make_string(const char* s, size_t n) {
        const uint32 index = _alloc_header();
        _Header* h = new (_jb.at(index)) _Header(kString);
        h->size = n;

        const uint32 body = _jb.alloc(_b8(n + 1));
        assert(body == index + 2);
        h = (_Header*) _jb.at(index);
        assert(h->size == n);

        char* p = (char*) _jb.at(h->index = body);
        memcpy(p, s, n);
        p[n] = '\0';
        return index;
    }

    uint32 _make_key(const char* x, size_t n) {
        const uint32 index = _jb.alloc(_b8(n + 1));
        char* p = (char*)_jb.at(index);
        memcpy(p, x, n);
        p[n] = '\0';
        return index;
    }

    uint32 _make_key(Key key) {
        const uint32 n = (uint32) strlen(key);
        const uint32 index = _jb.alloc(_b8(n + 1));
        memcpy(_jb.at(index), key, n + 1);
        return index;
    }

    uint32 _make_array(uint32 cap=0) {
        const uint32 index = _alloc_header();
        const uint32 q = cap == 0 ? 0 : _alloc_queue(cap);
        _Header* h = new (_jb.at(index)) _Header(kArray);
        h->size = 0;
        h->index = q;
        return index;
    }

    uint32 _make_object(uint32 cap=0) {
        const uint32 index = _alloc_header();
        const uint32 q = cap == 0 ? 0 : _alloc_queue(cap * 2);
        _Header* h = new (_jb.at(index)) _Header(kObject);
        h->size = 0;
        h->index = q;
        return index;
    }

    bool _is_null(uint32 index) const {
        return ((_Header*)_jb.at(index))->type == kNull;
    }

    bool _is_bool(uint32 index) const {
        return ((_Header*)_jb.at(index))->type == kBool;
    }

    bool _is_int(uint32 index) const {
        return ((_Header*)_jb.at(index))->type == kInt;
    }

    bool _is_double(uint32 index) const {
        return ((_Header*)_jb.at(index))->type == kDouble;
    }

    bool _is_string(uint32 index) const {
        return ((_Header*)_jb.at(index))->type == kString;
    }

    bool _is_array(uint32 index) const {
        return ((_Header*)_jb.at(index))->type == kArray;
    }

    bool _is_object(uint32 index) const {
        return ((_Header*)_jb.at(index))->type == kObject;
    }

    bool _get_bool(uint32 index) const {
        _Header* h = (_Header*)_jb.at(index);
        assert(h->type == kBool);
        return h->b;
    }

    int64 _get_int64(uint32 index) const {
        _Header* h = (_Header*)_jb.at(index);
        assert(h->type == kInt);
        return h->i;
    }

    double _get_double(uint32 index) const {
        _Header* h = (_Header*)_jb.at(index);
        assert(h->type == kDouble);
        return h->d;
    }

    const char* _get_string(uint32 index) const {
        _Header* h = (_Header*)_jb.at(index);
        assert(h->type == kString);
        return (const char*) _jb.at(h->index);
    }

    // add member to an object
    //   @key:   index of key
    //   @val:   index of value
    //   @index: index of the object
    void _add_member(uint32 key, uint32 val, uint32 index);

    Value _add_array(Key key, uint32 cap, uint32 index) {
        uint32 i = _make_array(cap);
        this->_add_member(_make_key(key), i, index);
        return Value(this, i);
    }

    Value _add_object(Key key, uint32 cap, uint32 index) {
        uint32 i = _make_object(cap);
        this->_add_member(_make_key(key), i, index);
        return Value(this, i);
    }

    // push element to an array
    //   @val:   index of value
    //   @index: index of the array
    void _push_back(uint32 val, uint32 index);

    Value _push_array(uint32 cap, uint32 index) {
        uint32 i = _make_array(cap);
        this->_push_back(i, index);
        return Value(this, i);
    }

    Value _push_object(uint32 cap, uint32 index) {
        uint32 i = _make_object(cap);
        this->_push_back(i, index);
        return Value(this, i);
    }

    uint32 _alloc_queue(uint32 cap) {
        uint32 index = _jb.alloc(_b8(16 + cap * sizeof(uint32)));
        new (_jb.at(index)) xx::Queue(cap);
        return index;
    }

    uint32 _alloc_queue(const char* p, size_t n) {
        const uint32 cap = (n >> 2);
        uint32 index = _jb.alloc(_b8(16 + n));
        char* s = (char*) new (_jb.at(index)) xx::Queue(cap, cap);
        memcpy(s + 16, p, n);
        return index;
    }

  private:
    struct _Header {
        _Header(Type t) : type(t) {}
        uint32 type;
        uint32 size;      // for string, array, object
        union {
            bool b;       // for bool
            int64 i;      // for int
            double d;     // for double
            uint32 index; // for string, array, object
        };
    }; // 16 bytes

    union {
        xx::JBlock _jb;
        void* _mem;
    };
};

inline bool Value::is_null() const {
    return _root->_is_null(_index);
}

inline bool Value::is_bool() const {
    return _root->_is_bool(_index);
}

inline bool Value::is_int() const {
    return _root->_is_int(_index);
}

inline bool Value::is_double() const {
    return _root->_is_double(_index);
}

inline bool Value::is_string() const {
    return _root->_is_string(_index);
}

inline bool Value::is_array() const {
    return _root->_is_array(_index);
}

inline bool Value::is_object() const {
    return _root->_is_object(_index);
}

inline bool Value::get_bool() const {
    return _root->_get_bool(_index);
}

inline int64 Value::get_int64() const {
    return _root->_get_int64(_index);
}

inline int Value::get_int() const {
    return (int) this->get_int64();
}

inline int32 Value::get_int32() const {
    return (int32) this->get_int64();
}

inline uint32 Value::get_uint32() const {
    return (uint32) this->get_int64();
}

inline uint64 Value::get_uint64() const {
    return (uint64) this->get_int64();
}

inline double Value::get_double() const {
    return _root->_get_double(_index);
}

inline const char* Value::get_string() const {
    return _root->_get_string(_index);
}

inline void Value::add_member(Key key, bool x) {
    return _root->_add_member(_root->_make_key(key), _root->_make_bool(x), _index);
}

inline void Value::add_member(Key key, int64 x) {
    return _root->_add_member(_root->_make_key(key), _root->_make_int(x), _index);
}

inline void Value::add_member(Key key, int x) {
    return this->add_member(key, (int64)x);
}

inline void Value::add_member(Key key, uint32 x) {
    return this->add_member(key, (int64)x);
}

inline void Value::add_member(Key key, uint64 x) {
    return this->add_member(key, (int64)x);
}

inline void Value::add_member(Key key, double x) {
    return _root->_add_member(_root->_make_key(key), _root->_make_double(x), _index);
}

inline void Value::add_member(Key key, const char* x, size_t n) {
    return _root->_add_member(_root->_make_key(key), _root->_make_string(x, n), _index);
}

inline void Value::add_member(Key key, const char* x) {
    return this->add_member(key, x, strlen(x));
}

inline void Value::add_member(Key key, const std::string& x) {
    return this->add_member(key, x.data(), x.size());
}

inline void Value::add_member(Key key, const fastring& x) {
    return this->add_member(key, x.data(), x.size());
}

inline void Value::add_null(Key key) {
    return _root->_add_member(_root->_make_key(key), _root->_make_null(), _index);
}

inline Value Value::add_array(Key key, uint32 cap) {
    return _root->_add_array(key, cap, _index);
}

inline Value Value::add_object(Key key, uint32 cap) {
    return _root->_add_object(key, cap, _index);
}

inline void Value::push_back(bool x) {
    return _root->_push_back(_root->_make_bool(x), _index);
}

inline void Value::push_back(int64 x) {
    return _root->_push_back(_root->_make_int(x), _index);
}

inline void Value::push_back(int x) {
    return this->push_back((int64)x);
}

inline void Value::push_back(uint32 x) {
    return this->push_back((int64)x);
}

inline void Value::push_back(uint64 x) {
    return this->push_back((int64)x);
}

inline void Value::push_back(double x) {
    return _root->_push_back(_root->_make_double(x), _index);
}

inline void Value::push_back(const char* x, size_t n) {
    return _root->_push_back(_root->_make_string(x, n), _index);
}

inline void Value::push_back(const char* x) {
    return this->push_back(x, strlen(x));
}

inline void Value::push_back(const std::string& x) {
    return this->push_back(x.data(), x.size());
}

inline void Value::push_back(const fastring& x) {
    return this->push_back(x.data(), x.size());
}

inline void Value::push_null() {
    return _root->_push_back(_root->_make_null(), _index);
}

inline Value Value::push_array(uint32 cap) {
    return _root->_push_array(cap, _index);
}

inline Value Value::push_object(uint32 cap) {
    return _root->_push_object(cap, _index);
}

inline Value Value::operator[](uint32 i) const {
    return _root->_at(i, _index);
}

inline Value Value::operator[](int i) const {
    return this->operator[]((uint32)i);
}

inline Value Value::operator[](Key key) const {
    return _root->_at(key, _index);
}

inline bool Value::has_member(Key key) const {
    return _root->_has_member(key, _index);
}

inline uint32 Value::size() const {
    return _root->_size(_index);
}

inline Value::iterator Value::begin() const {
    return _root->_begin(_index);
}

inline Value::iterator::iterator(Root* root, uint32 index, uint32 type)
    : _root(root), _index(index), _pos(0) {
    _step = (type == Root::kObject ? 2 : 1);
}

inline bool Value::iterator::operator!=(const End&) const {
    return _index != 0;
}

inline bool Value::iterator::operator==(const End&) const {
    return _index == 0;
}

inline Value::iterator& Value::iterator::operator++() {
    xx::Queue* a = (xx::Queue*) _root->_jb.at(_index);
    if ((_pos += _step) >= a->size) {
        _index = a->next;
        _pos = 0;
    }
    return *this;
}

inline Value Value::iterator::operator*() const {
    const uint32 index = ((xx::Queue*)_root->_jb.at(_index))->p[_pos];
    return Value(_root, index);
}

inline Value::Key Value::iterator::key() const {
    const uint32 index = ((xx::Queue*)_root->_jb.at(_index))->p[_pos];
    return (const char*)_root->_jb.at(index);
}

inline Value Value::iterator::value() const {
    const uint32 index = ((xx::Queue*)_root->_jb.at(_index))->p[_pos + 1];
    return Value(_root, index);
}

inline void Value::str(fastream& fs) const {
    return _root->_Json2str(fs, false, _index);
}

inline fastring Value::str(uint32 cap) const {
    fastring s(cap);
    this->str(*(fastream*)&s);
    return std::move(s);
}

inline void Value::dbg(fastream& fs) const {
    return _root->_Json2str(fs, true, _index);
}

inline fastring Value::dbg(uint32 cap) const {
    fastring s(cap);
    this->dbg(*(fastream*)&s);
    return std::move(s);
}

inline fastring Value::pretty(int indent) const {
    fastring s(256);
    _root->_Json2pretty(*(fastream*)&s, indent, indent, _index);
    return std::move(s);
}

// return an empty array
inline Root array() {
    return Root(Root::TypeArray());
}

// return an empty object
inline Root object() {
    return Root(Root::TypeObject());
}

inline Root parse(const char* s, size_t n) {
    void* p = 0;
    if (((Root*)&p)->parse_from(s, n)) return std::move(*(Root*)&p);
    return Root();
}

inline Root parse(const char* s) {
    return parse(s, strlen(s));
}

inline Root parse(const fastring& s) {
    return parse(s.data(), s.size());
}

inline Root parse(const std::string& s) {
    return parse(s.data(), s.size());
}

} // json

typedef json::Root Json;

inline fastream& operator<<(fastream& fs, const json::Root& x) {
    x.dbg(fs);
    return fs;
}

inline fastream& operator<<(fastream& fs, const json::Value& x) {
    x.dbg(fs);
    return fs;
}
