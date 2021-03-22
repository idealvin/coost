#pragma once

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

    ~JBlock() { free(_h); }

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

    void clear() { _h->size = 0; }

    void reserve(uint32 n) {
        if (n > _h->cap) {
            _h->cap = n;
            _h = (_Header*) realloc(_h, sizeof(_Header) + n * N);
            assert(_h);
        }
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

    fastream& alloc_stream() { _fs.clear(); return _fs; }
    fastream& alloc_stack()  { return _stack; } 

  private:
    std::vector<void*> _jb;
    fastream _fs;    // for parsing string
    fastream _stack; // for parsing array, object
};

inline JAlloc* jalloc() {
    static __thread JAlloc* alloc = 0;
    return alloc ? alloc : (alloc = new JAlloc);
}

// Queue holds index of elements in array and object.
// Every array and object owns one or more Queue.
// @next: points to the next Queue.
struct Queue {
    Queue(uint32 n)           : cap(n), size(0), next(0) {}
    Queue(uint32 n, uint32 m) : cap(n), size(m), next(0) {}
    ~Queue() = default;

    void push(uint32 x)           { p[size++] = x; }
    void push(uint32 x, uint32 y) { p[size++] = x; p[size++] = y; }
    bool full() const { return size >= cap; }

    uint32 cap;
    uint32 size;
    uint32 next;
    uint32 dummy;
    uint32 p[];
};

} // xx

// root node of the json
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

    friend class Parser;
    struct TypeArray {};
    struct TypeObject {};
    typedef const char* Key;
    typedef const char* S;

    // sub node of Root.
    // A Value must be created from a Root.
    class Value {
      public:
        ~Value() = default;

        bool is_null()   const { return _root->_is_null(_index); }
        bool is_bool()   const { return _root->_is_bool(_index); }
        bool is_int()    const { return _root->_is_int(_index); }
        bool is_double() const { return _root->_is_double(_index); }
        bool is_string() const { return _root->_is_string(_index); }
        bool is_array()  const { return _root->_is_array(_index); }
        bool is_object() const { return _root->_is_object(_index); }

        bool get_bool()     const { return _root->_get_bool(_index); }
        int64 get_int64()   const { return _root->_get_int64(_index); }
        int get_int()       const { return (int)   this->get_int64(); }
        int32 get_int32()   const { return (int32) this->get_int64(); }
        uint32 get_uint32() const { return (uint32)this->get_int64(); }
        uint64 get_uint64() const { return (uint64)this->get_int64(); }
        double get_double() const { return _root->_get_double(_index); }
        S get_string()      const { return _root->_get_string(_index); }

        void add_member(Key key, bool x)   { return _root->_add_member(key, x, _index); }
        void add_member(Key key, int64 x)  { return _root->_add_member(key, x, _index); }
        void add_member(Key key, int x)    { return this->add_member(key, (int64)x); }
        void add_member(Key key, uint32 x) { return this->add_member(key, (int64)x); }
        void add_member(Key key, uint64 x) { return this->add_member(key, (int64)x); }
        void add_member(Key key, double x) { return _root->_add_member(key, x, _index); }

        void add_member(Key key, S x, size_t n)        { return _root->_add_member(key, x, n, _index); }
        void add_member(Key key, S x)                  { return this->add_member(key, x, strlen(x)); }
        void add_member(Key key, const std::string& x) { return this->add_member(key, x.data(), x.size()); }
        void add_member(Key key, const fastring& x)    { return this->add_member(key, x.data(), x.size()); }

        void add_null(Key key)                  { return _root->_add_null(key, _index); }
        Value add_array (Key key, uint32 cap=0) { return _root->_add_array (key, cap, _index); }
        Value add_object(Key key, uint32 cap=0) { return _root->_add_object(key, cap, _index); }

        void push_back(bool x)   { return _root->_push_back(x, _index); }
        void push_back(int64 x)  { return _root->_push_back(x, _index); }
        void push_back(int x)    { return this->push_back((int64)x); }
        void push_back(uint32 x) { return this->push_back((int64)x); }
        void push_back(uint64 x) { return this->push_back((int64)x); }
        void push_back(double x) { return _root->_push_back(x, _index); }

        void push_back(S x, size_t n)        { return _root->_push_back(x, n, _index); }
        void push_back(S x)                  { return this->push_back(x, strlen(x)); }
        void push_back(const std::string& x) { return this->push_back(x.data(), x.size()); }
        void push_back(const fastring& x)    { return this->push_back(x.data(), x.size()); }

        void push_null()                { return _root->_push_null(_index); }
        Value push_array (uint32 cap=0) { return _root->_push_array (cap, _index); }
        Value push_object(uint32 cap=0) { return _root->_push_object(cap, _index); }

        Value operator[](uint32 i) const { return _root->_at(i, _index); }
        Value operator[](int i)    const { return this->operator[]((uint32)i); }
        Value operator[](Key key)  const { return _root->_at(key, _index); }
        bool has_member(Key key)   const { return _root->_has_member(key, _index); }
        uint32 size()              const { return _root->_size(_index); }

        fastream& str(fastream& fs)     const { return _root->_Json2str(fs, false, _index); }
        fastream& dbg(fastream& fs)     const { return _root->_Json2str(fs, true, _index); }
        fastream& pretty(fastream& fs)  const { return _root->_Json2pretty(fs, 4, 4, _index); }
        fastring str(uint32 cap=256)    const { fastream s(cap); return std::move(*(fastring*) &this->str(s)); }
        fastring dbg(uint32 cap=256)    const { fastream s(cap); return std::move(*(fastring*) &this->dbg(s)); }
        fastring pretty(uint32 cap=256) const { fastream s(cap); return std::move(*(fastring*) &this->pretty(s)); }

        class iterator {
          public:
            iterator(Root* root, uint32 q, uint32 type)
                : _root(root), _q(q), _i(0) {
                _step = (type == Root::kObject ? 2 : 1);
            }

            struct End {};
            static const End& end() {
                static End kEnd;
                return kEnd;
            }

            bool operator!=(const End&) const { return _q != 0; }
            bool operator==(const End&) const { return _q == 0; }

            iterator& operator++() {
                xx::Queue* a = (xx::Queue*) _root->_p8(_q);
                if ((_i += _step) >= a->size) {
                    _q = a->next;
                    _i = 0;
                }
                return *this;
            }

            Value operator*() const { return Value(_root, ((xx::Queue*)_root->_p8(_q))->p[_i]); }
            Key key()         const { return (Key)_root->_p8(((xx::Queue*)_root->_p8(_q))->p[_i]); }
            Value value()     const { return Value(_root, ((xx::Queue*)_root->_p8(_q))->p[_i + 1]); }

          private:
            Root* _root;
            uint32 _q; // index of the Queue
            uint32 _i; // position in the Queue
            uint32 _step;
        };

        iterator begin() const { return _root->_begin(_index); }
        const iterator::End& end() const { return iterator::end(); }

      private:
        Root* _root;
        uint32 _index;

        friend class Root;
        Value(Root* root, uint32 index)
            : _root(root), _index(index) {
        }
    };

    typedef Value::iterator iterator;

    Root()           : _mem(xx::jalloc()->alloc_jblock()) { _make_null(); }
    Root(TypeArray)  : _mem(xx::jalloc()->alloc_jblock()) { _make_array(); }
    Root(TypeObject) : _mem(xx::jalloc()->alloc_jblock()) { _make_object(); }
    Root(Root&& r) noexcept : _mem(r._mem) { r._mem = 0; }
    ~Root() { if (_mem) xx::jalloc()->dealloc_jblock(_mem); }

    Root(const Root& r) = delete;
    Root& operator=(const Root& r) = delete;

    Root& operator=(Root&& r) noexcept {
        if (&r != this) {
            if (_mem) xx::jalloc()->dealloc_jblock(_mem);
            _mem = r._mem;
            r._mem = 0;
        }
        return *this;
    }

    void add_member(Key key, bool x)   { return this->_add_member(key, x, 0); }
    void add_member(Key key, int64 x)  { return this->_add_member(key, x, 0); }
    void add_member(Key key, int x)    { return this->add_member(key, (int64)x); }
    void add_member(Key key, uint32 x) { return this->add_member(key, (int64)x); }
    void add_member(Key key, uint64 x) { return this->add_member(key, (int64)x); }
    void add_member(Key key, double x) { return this->_add_member(key, x, 0); }

    void add_member(Key key, S x, size_t n)        { return this->_add_member(key, x, n, 0); }
    void add_member(Key key, S x)                  { return this->add_member(key, x, strlen(x)); }
    void add_member(Key key, const std::string& x) { return this->add_member(key, x.data(), x.size()); }
    void add_member(Key key, const fastring& x)    { return this->add_member(key, x.data(), x.size()); }

    void add_null(Key key)                  { return this->_add_null(key, 0); }
    Value add_array (Key key, uint32 cap=0) { return this->_add_array (key, cap, 0); }
    Value add_object(Key key, uint32 cap=0) { return this->_add_object(key, cap, 0); }

    void push_back(bool x)   { return this->_push_back(x, 0); }
    void push_back(int64 x)  { return this->_push_back(x, 0); }
    void push_back(int x)    { return this->push_back((int64)x); }
    void push_back(uint32 x) { return this->push_back((int64)x); }
    void push_back(uint64 x) { return this->push_back((int64)x); }
    void push_back(double x) { return this->_push_back(x, 0); }

    void push_back(S x, size_t n)        { return this->_push_back(x, n, 0); }
    void push_back(S x)                  { return this->push_back(x, strlen(x)); }
    void push_back(const std::string& x) { return this->push_back(x.data(), x.size()); }
    void push_back(const fastring& x)    { return this->push_back(x.data(), x.size()); }

    void push_null()                { return this->_push_null(0); }
    Value push_array (uint32 cap=0) { return this->_push_array (cap, 0); }
    Value push_object(uint32 cap=0) { return this->_push_object(cap, 0); }

    bool is_null()   const { return this->_is_null(0); }
    bool is_bool()   const { return this->_is_bool(0); }
    bool is_int()    const { return this->_is_int(0); }
    bool is_double() const { return this->_is_double(0); }
    bool is_string() const { return this->_is_string(0); }
    bool is_array()  const { return this->_is_array(0); }
    bool is_object() const { return this->_is_object(0); }

    bool get_bool()     const { return this->_get_bool(0); }
    int64 get_int64()   const { return this->_get_int64(0); }
    int get_int()       const { return (int)this->get_int64(); }
    int32 get_int32()   const { return (int32)this->get_int64(); }
    uint32 get_uint32() const { return (uint32)this->get_int64(); }
    uint64 get_uint64() const { return (uint64)this->get_int64(); }
    double get_double() const { return this->_get_double(0); }
    S get_string()      const { return this->_get_string(0); }

    iterator begin()           const { return this->_begin(0); }
    const iterator::End& end() const { return iterator::end(); }
    Value operator[](uint32 i) const { return this->_at(i, 0); }
    Value operator[](int i)    const { return this->operator[]((uint32)i); }
    Value operator[](Key key)  const { return this->_at(key, 0); }
    bool has_member(Key key)   const { return this->_has_member(key, 0); }
    uint32 size()              const { return this->_size(0); }

    // Stringify.
    //   - str() converts Json to string without any white spaces.
    //   - dbg() behaves the same as str() except that it will truncate the 
    //     string type if its length is greater than 256 bytes.
    //   - pretty() converts Json to human readable string.
    //
    //   - @cap  memory allocated when fastream is initialized, which can be 
    //     used to reduce memory reallocation.
    fastream& str(fastream& fs)     const { return this->_Json2str(fs, false, 0); }
    fastream& dbg(fastream& fs)     const { return this->_Json2str(fs, true, 0); }
    fastream& pretty(fastream& fs)  const { return this->_Json2pretty(fs, 4, 4, 0); }
    fastring str(uint32 cap=256)    const { fastream s(cap); return std::move(*(fastring*) &this->str(s)); }
    fastring dbg(uint32 cap=256)    const { fastream s(cap); return std::move(*(fastring*) &this->dbg(s)); }
    fastring pretty(uint32 cap=256) const { fastream s(cap); return std::move(*(fastring*) &this->pretty(s)); }

    // Parse Json from string, inverse to stringify.
    bool parse_from(const char* s, size_t n);
    bool parse_from(const char* s)        { return this->parse_from(s, strlen(s)); }
    bool parse_from(const fastring& s)    { return this->parse_from(s.data(), s.size()); }
    bool parse_from(const std::string& s) { return this->parse_from(s.data(), s.size()); }

  private:
    fastream& _Json2str(fastream& fs, bool debug, uint32 index) const;
    fastream& _Json2pretty(fastream& fs, int indent, int n, uint32 index) const;

    Value _at(uint32 i, uint32 index) const;
    Value _at(Key key, uint32 index) const;
    bool _has_member(Key key, uint32 index) const;
    uint32 _size(uint32 index) const;

    iterator _begin(uint32 index) const {
        _Header* h = (_Header*) _p8(index);
        assert(h->type & (kObject | kArray));
        return iterator((Root*)this, h->index, h->type);
    }

    // _b8() calculate blocks num from bytes, _b8(15) = 2, etc.
    // _p8() return pointer at the index, 8-byte aligned.
    // _a8() allocate blocks according to the length.
    uint32 _b8(uint32 n) const { return (uint32)((n >> 3) + !!(n & 7)); }
    void*  _p8(uint32 i) const { return _jb.at(i); }
    uint32 _a8(size_t n)       { return _jb.alloc(_b8(n)); }
    uint32 _alloc_header()     { return _jb.alloc(2); }

    uint32 _make_null()           { uint32 i = _alloc_header();  new (_p8(i)) _Header(kNull); return i; }
    uint32 _make_bool(bool x)     { uint32 i = _alloc_header(); (new (_p8(i)) _Header(kBool))->b = x; return i; }
    uint32 _make_int(int64 x)     { uint32 i = _alloc_header(); (new (_p8(i)) _Header(kInt))->i = x; return i; }
    uint32 _make_double(double x) { uint32 i = _alloc_header(); (new (_p8(i)) _Header(kDouble))->d = x; return i; }

    uint32 _make_string(const char* s, size_t n) {
        const uint32 head = _alloc_header();
        const uint32 body = _a8(n + 1);
        _Header* h = new (_p8(head)) _Header(kString);
        h->size = (uint32)n; // length of the string
        h->index = body;     // points to the body of the string
        char* p = (char*)_p8(body);
        memcpy(p, s, n);
        p[n] = '\0';
        return head;
    }

    uint32 _make_key(const char* x, size_t n) {
        const uint32 index = _a8(n + 1);
        char* p = (char*)_p8(index);
        memcpy(p, x, n);
        p[n] = '\0';
        return index;
    }

    uint32 _make_key(Key key) {
        const uint32 n = (uint32) strlen(key);
        const uint32 index = _a8(n + 1);
        memcpy(_p8(index), key, n + 1);
        return index;
    }

    uint32 _make_array(uint32 cap=0) {
        const uint32 head = _alloc_header();
        const uint32 q = (cap == 0 ? 0 : _alloc_queue(cap));
        (new (_p8(head)) _Header(kArray))->index = q;
        return head;
    }

    uint32 _make_object(uint32 cap=0) {
        const uint32 head = _alloc_header();
        const uint32 q = (cap == 0 ? 0 : _alloc_queue(cap * 2));
        (new (_p8(head)) _Header(kObject))->index = q;
        return head;
    }

    bool _is_null(uint32 i)   const { return ((_Header*)_p8(i))->type == kNull; }
    bool _is_bool(uint32 i)   const { return ((_Header*)_p8(i))->type == kBool; }
    bool _is_int(uint32 i)    const { return ((_Header*)_p8(i))->type == kInt; }
    bool _is_double(uint32 i) const { return ((_Header*)_p8(i))->type == kDouble; }
    bool _is_string(uint32 i) const { return ((_Header*)_p8(i))->type == kString; }
    bool _is_array (uint32 i) const { return ((_Header*)_p8(i))->type == kArray; }
    bool _is_object(uint32 i) const { return ((_Header*)_p8(i))->type == kObject; }

    bool _get_bool(uint32 i)     const { _Header* h = (_Header*)_p8(i); assert(h->type == kBool); return h->b; }
    int64 _get_int64(uint32 i)   const { _Header* h = (_Header*)_p8(i); assert(h->type == kInt); return h->i; }
    double _get_double(uint32 i) const { _Header* h = (_Header*)_p8(i); assert(h->type == kDouble); return h->d; }
    S _get_string(uint32 i)      const { _Header* h = (_Header*)_p8(i); assert(h->type == kString); return (S)_p8(h->index); }

    // add member to an object
    //   @key:   index of key
    //   @val:   index of value
    //   @i:     index of the object
    void _add_member(uint32 key, uint32 val, uint32 i);

    void _add_member(Key key, bool x, uint32 i) {
        return this->_add_member(_make_key(key), _make_bool(x), i);
    }
    void _add_member(Key key, int64 x, uint32 i) {
        return this->_add_member(_make_key(key), _make_int(x), i);
    }
    void _add_member(Key key, double x, uint32 i) {
        return this->_add_member(_make_key(key), _make_double(x), i);
    }
    void _add_member(Key key, const char* x, size_t n, uint32 i) {
        return this->_add_member(_make_key(key), _make_string(x, n), i);
    }

    void _add_null(Key key, uint32 i) {
        return this->_add_member(_make_key(key), _make_null(), i);
    }

    Value _add_array(Key key, uint32 cap, uint32 i) {
        uint32 a = _make_array(cap);
        this->_add_member(_make_key(key), a, i);
        return Value(this, a);
    }

    Value _add_object(Key key, uint32 cap, uint32 i) {
        uint32 o = _make_object(cap);
        this->_add_member(_make_key(key), o, i);
        return Value(this, o);
    }

    // push element to an array
    //   @val:   index of value
    //   @i:     index of the array
    void _push_back(uint32 val, uint32 i);
    void _push_back(bool x, uint32 i)        { return this->_push_back(_make_bool(x), i); }
    void _push_back(int64 x, uint32 i)       { return this->_push_back(_make_int(x), i); }
    void _push_back(double x, uint32 i)      { return this->_push_back(_make_double(x), i); }
    void _push_back(S x, size_t n, uint32 i) { return this->_push_back(_make_string(x, n), i); }
    void _push_null(uint32 i)                { return this->_push_back(_make_null(), i); }
    Value _push_array (uint32 cap, uint32 i) { uint32 a = _make_array (cap); this->_push_back(a, i); return Value(this, a); }
    Value _push_object(uint32 cap, uint32 i) { uint32 o = _make_object(cap); this->_push_back(o, i); return Value(this, o); }

    uint32 _alloc_queue(uint32 cap) {
        uint32 index = _a8(16 + cap * sizeof(uint32));
        new (_p8(index)) xx::Queue(cap);
        return index;
    }

    uint32 _alloc_queue(const char* p, size_t n) {
        const uint32 cap = (n >> 2); // n/4
        uint32 index = _a8(16 + n);
        char* s = (char*) new (_p8(index)) xx::Queue(cap, cap);
        memcpy(s + 16, p, n);
        return index;
    }

  private:
    struct _Header {
        _Header(Type t) : type(t) {}
        uint32 type;
        uint32 size;      // for string
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

typedef Root::Value Value;

// json::array()  creates an empty array
// json::object() creates an empty object
inline Root array()  { return Root(Root::TypeArray()); }
inline Root object() { return Root(Root::TypeObject()); }

inline Root parse(const char* s, size_t n) {
    void* p = 0;
    if (((Root*)&p)->parse_from(s, n)) return std::move(*(Root*)&p);
    return std::move((*(Root*)&p) = Root());
}

inline Root parse(const char* s)        { return parse(s, strlen(s)); }
inline Root parse(const fastring& s)    { return parse(s.data(), s.size()); }
inline Root parse(const std::string& s) { return parse(s.data(), s.size()); }

} // json

typedef json::Root Json;

inline fastream& operator<<(fastream& fs, const json::Root& x)  { return x.dbg(fs); }
inline fastream& operator<<(fastream& fs, const json::Value& x) { return x.dbg(fs); }
