#pragma once

#include "log.h"
#include "fastream.h"
#include <vector>

namespace jsonv2 {
namespace xx {

// JBlock is an array of uint64.
// json::Root will be parsed or constructed as a JBlock.
class JBlock {
  public:
    explicit JBlock(size_t cap) {
        if (cap < 8) cap = 8;
        _h = (_Header*) malloc(sizeof(_Header) + 8 * cap);
        _h->cap = (uint32)cap;
        _h->size = 0;
    }

    ~JBlock() {
        free(_h);
    }

    // alloc 8n bytes, return the index
    uint32 alloc(uint32 n) {
        uint32 index = _h->size;
        if ((_h->size += n) > _h->cap) {
            _h->cap += (_h->cap >> 1);
            _h = (_Header*) realloc(_h, sizeof(_Header) + 8 * _h->cap);
            assert(_h);
        }
        return index;
    }

    void* at(uint32 index) const {
        return _h->p + index;
    }

    void reserve(uint32 n) {
        if (n > _h->cap) {
            _h = (_Header*) realloc(_h, sizeof(_Header) + 8 * n);
            _h->cap = n;
        }
    }

    uint32 size() const {
        return _h->size;
    }

    void clear() {
        _h->size = 0;
    }

    void copy_from(const JBlock& m) {
        this->reserve(m._h->cap);
        memcpy(_h, m._h, 8 + 8 * m._h->size);
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

    void* alloc_jblock(size_t cap=32) {
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

struct Array {
    Array(uint32 n)
        : cap(n), size(0), next(0) {
    }

    Array(uint32 n, uint32 m)
        : cap(n), size(m), next(0) {
    }

    ~Array() = default;

    typedef uint32 T;

    void push(T x) {
        p[size++] = x;
    }

    void push(T x, T y) {
        p[size++] = x;
        p[size++] = y;
    }

    bool full() const {
        return size >= cap;
    }

    uint32 cap;
    uint32 size;
    uint32 next;
    uint32 tail;
    uint32 p[];
};

} // xx

// root node of the json
class Root;

// sub node of Root.
// A Node must be created from a Root.
class Node {
  public:
    ~Node() = default;

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
    Node add_array(Key key);
    Node add_object(Key key);

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
    Node push_array();
    Node push_object();

    Node operator[](uint32 i) const;
    Node operator[](int i) const;
    Node operator[](Key key) const;
    bool has_member(Key key) const;

    class iterator {
      public:
        iterator(Root* root, uint32 index, uint32 type);

        Key key() const;
        Node value() const;

        Node operator*() const;
        iterator& operator++();

        struct End {};
        static const End& end() {
            static End kEnd;
            return kEnd;
        }

        bool operator==(const End&) const;
        bool operator!=(const End&) const;
        bool has_next() const;

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

  private:
    Root* _root;
    uint32 _index;

    friend class Root;
    Node(Root* root, uint32 index)
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

    friend class Node;
    friend class Parser;
    friend class Node::iterator;
    struct TypeArray {};
    struct TypeObject {};
    typedef const char* Key;
    typedef Node::iterator iterator;

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
    Root& operator=(Root&& r) = delete;

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

    Node add_array(Key key) {
        return this->_add_array(key, 0);
    }

    Node add_object(Key key) {
        return this->_add_object(key, 0);
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

    Node push_array() {
        return this->_push_array(0);
    }

    Node push_object() {
        return this->_push_object(0);
    }

    bool is_null() const {
        return this->_is_null(0);
    }

    bool is_array() const {
        return this->_is_array(0);
    }

    bool is_object() const {
        return this->_is_object(0);
    }

    fastring str(uint32 cap=256) const {
        fastring s(cap);
        this->_Json2str(*(fastream*)&s);
        return std::move(s);
    }

    // write json string to fastream
    void str(fastream& fs) const {
        this->_Json2str(fs);
    }

    // json to debug string
    fastring dbg(uint32 cap=256) const {
        fastring s(cap);
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

    iterator begin() const {
        return this->_begin(0);
    }

    const iterator::End& end() const {
        return iterator::end();
    }

    Node operator[](uint32 i) const {
        return this->_at(i, 0);
    }

    Node operator[](int i) const {
        return this->operator[]((uint32)i);
    }

    Node operator[](Key key) const {
        return this->_at(key, 0);
    }

    bool has_member(Key key) const {
        return this->_has_member(key, 0);
    }

  private:
    void _Json2str(fastream& fs, bool debug=false) const;
    void _Json2pretty(fastream& fs, int indent, int n) const;

    Node _at(uint32 i, uint32 index) const;
    Node _at(Key key, uint32 index) const;
    bool _has_member(Key key, uint32 index) const;

    uint32 _b8(uint32 n) {
        return (uint32)((n >> 3) + !!(n & 7));
    }

    iterator _begin(uint32 index) const {
        assert(_mem);
        _Header* h = (_Header*) _jb.at(index);
        assert(h->type == kObject || h->type == kArray);
        return iterator((Root*)this, h->index, h->type);
    }

    uint32 _make_null() {
        const uint32 index = _jb.alloc(1);
        new (_jb.at(index)) _Header(kNull);
        return index;
    }

    // | type: 4 | val: 4 |
    uint32 _make_bool(bool x) {
        const uint32 index = _jb.alloc(1);
        (new (_jb.at(index)) _Header(kBool))->b = x;
        return index;
    }

    // | type: 4 | 4 | val: 8 |
    uint32 _make_int(int64 x) {
        const uint32 index = _jb.alloc(2);
        (new (_jb.at(index)) _Header(kInt))->i = x;
        return index;
    }

    // | type: 4 | 4 | val: 8 |
    uint32 _make_double(double x) {
        const uint32 index = _jb.alloc(2);
        (new (_jb.at(index)) _Header(kDouble))->d = x;
        return index;
    }

    // | type: 4 | slen: 4 | body |
    uint32 _make_string(const char* s, size_t n) {
        const uint32 index = _jb.alloc(_b8(n + 1 + 8));
        _Header* h = new (_jb.at(index)) _Header(kString);
        h->slen = n;
        char* p = (char*)h + 8;
        memcpy(p, s, n);
        p[n] = '\0';
        return index;
    }

    // | 4 | length: 4 |
    uint32 _make_key(const char* x, size_t n) {
        const uint32 index = _jb.alloc(_b8(n + 1 + 8));
        char* p = (char*)_jb.at(index) + 8;
        ((uint32*)p)[-1] = n;
        memcpy(p, x, n);
        p[n] = '\0';
        return index;
    }

    uint32 _make_key(Key key) {
        const uint32 n = (uint32) strlen(key);
        const uint32 index = _jb.alloc(_b8(n + 1 + 8));
        uint32* p = (uint32*)_jb.at(index) + 2;
        p[-1] = n;
        memcpy(p, key, n + 1);
        return index;
    }

    // |type: 4|index: 4|
    uint32 _make_array() {
        const uint32 index = _jb.alloc(1);
        _Header* h = new (_jb.at(index)) _Header(kArray);
        h->index = 0;
        return index;
    }

    // |type: 4|index: 4|
    uint32 _make_object() {
        const uint32 index = _jb.alloc(1);
        _Header* h = new (_jb.at(index)) _Header(kObject);
        h->index = 0;
        return index;
    }

    bool _is_null(uint32 index) const {
        return _mem == 0 || ((_Header*)_jb.at(index))->type == kNull;
    }

    bool _is_bool(uint32 index) const {
        return _mem && ((_Header*)_jb.at(index))->type == kBool;
    }

    bool _is_int(uint32 index) const {
        return _mem && ((_Header*)_jb.at(index))->type == kInt;
    }

    bool _is_double(uint32 index) const {
        return _mem && ((_Header*)_jb.at(index))->type == kDouble;
    }

    bool _is_string(uint32 index) const {
        return _mem && ((_Header*)_jb.at(index))->type == kString;
    }

    bool _is_array(uint32 index) const {
        return _mem && ((_Header*)_jb.at(index))->type == kArray;
    }

    bool _is_object(uint32 index) const {
        return _mem && ((_Header*)_jb.at(index))->type == kObject;
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
        return (char*)h + 8;
    }

    void _add_member(uint32 key, uint32 val, uint32 index);

    Node _add_array(Key key, uint32 index) {
        uint32 i = _make_array();
        this->_add_member(_make_key(key), i, index);
        return Node(this, i);
    }

    Node _add_object(Key key, uint32 index) {
        uint32 i = _make_object();
        this->_add_member(_make_key(key), i, index);
        return Node(this, i);
    }

    uint32 _alloc_array(uint32 cap) {
        uint32 index = _jb.alloc(_b8(16 + cap * sizeof(uint32)));
        new (_jb.at(index)) xx::Array(cap);
        return index;
    }

    uint32 _alloc_array(const char* p, size_t n) {
        const uint32 cap = (n >> 3);
        uint32 index = _jb.alloc(cap + 2);
        char* s = (char*) new (_jb.at(index)) xx::Array(cap, cap);
        memcpy(s + 16, p, n);
        return index;
    }

    void _push_back(uint32 val, uint32 index);

    Node _push_array(uint32 index) {
        uint32 i = _make_array();
        this->_push_back(i, index);
        return Node(this, i);
    }

    Node _push_object(uint32 index) {
        uint32 i = _make_object();
        this->_push_back(i, index);
        return Node(this, i);
    }

    uint32 _find(Key key, uint32 index) const;

  private:
    struct _Header {
        _Header(Type t) : type(t) {}
        uint32 type;
        union {
            bool b;       // for bool
            uint32 slen;  // for string
            uint32 index; // for array, object
        };
        union {
            int64 i;         // for int
            double d;        // for double
        };
    }; // 16 bytes

    _Header* _header() const {
        return (_Header*)_jb.at(0);
    }

    union {
        xx::JBlock _jb;
        void* _mem;
    };
};

inline bool Node::is_null() const {
    return _root->_is_null(_index);
}

inline bool Node::is_bool() const {
    return _root->_is_bool(_index);
}

inline bool Node::is_int() const {
    return _root->_is_int(_index);
}

inline bool Node::is_double() const {
    return _root->_is_double(_index);
}

inline bool Node::is_string() const {
    return _root->_is_string(_index);
}

inline bool Node::is_array() const {
    return _root->_is_array(_index);
}

inline bool Node::is_object() const {
    return _root->_is_object(_index);
}

inline bool Node::get_bool() const {
    return _root->_get_bool(_index);
}

inline int64 Node::get_int64() const {
    return _root->_get_int64(_index);
}

inline int Node::get_int() const {
    return (int) this->get_int64();
}

inline int32 Node::get_int32() const {
    return (int32) this->get_int64();
}

inline uint32 Node::get_uint32() const {
    return (uint32) this->get_int64();
}

inline uint64 Node::get_uint64() const {
    return (uint64) this->get_int64();
}

inline double Node::get_double() const {
    return _root->_get_double(_index);
}

inline const char* Node::get_string() const {
    return _root->_get_string(_index);
}

inline void Node::add_member(Key key, bool x) {
    return _root->_add_member(_root->_make_key(key), _root->_make_bool(x), _index);
}

inline void Node::add_member(Key key, int64 x) {
    return _root->_add_member(_root->_make_key(key), _root->_make_int(x), _index);
}

inline void Node::add_member(Key key, int x) {
    return this->add_member(key, (int64)x);
}

inline void Node::add_member(Key key, uint32 x) {
    return this->add_member(key, (int64)x);
}

inline void Node::add_member(Key key, uint64 x) {
    return this->add_member(key, (int64)x);
}

inline void Node::add_member(Key key, double x) {
    return _root->_add_member(_root->_make_key(key), _root->_make_double(x), _index);
}

inline void Node::add_member(Key key, const char* x, size_t n) {
    return _root->_add_member(_root->_make_key(key), _root->_make_string(x, n), _index);
}

inline void Node::add_member(Key key, const char* x) {
    return this->add_member(key, x, strlen(x));
}

inline void Node::add_member(Key key, const std::string& x) {
    return this->add_member(key, x.data(), x.size());
}

inline void Node::add_member(Key key, const fastring& x) {
    return this->add_member(key, x.data(), x.size());
}

inline void Node::add_null(Key key) {
    return _root->_add_member(_root->_make_key(key), _root->_make_null(), _index);
}

inline Node Node::add_array(Key key) {
    return _root->_add_array(key, _index);
}

inline Node Node::add_object(Key key) {
    return _root->_add_object(key, _index);
}

inline void Node::push_back(bool x) {
    return _root->_push_back(_root->_make_bool(x), _index);
}

inline void Node::push_back(int64 x) {
    return _root->_push_back(_root->_make_int(x), _index);
}

inline void Node::push_back(int x) {
    return this->push_back((int64)x);
}

inline void Node::push_back(uint32 x) {
    return this->push_back((int64)x);
}

inline void Node::push_back(uint64 x) {
    return this->push_back((int64)x);
}

inline void Node::push_back(double x) {
    return _root->_push_back(_root->_make_double(x), _index);
}

inline void Node::push_back(const char* x, size_t n) {
    return _root->_push_back(_root->_make_string(x, n), _index);
}

inline void Node::push_back(const char* x) {
    return this->push_back(x, strlen(x));
}

inline void Node::push_back(const std::string& x) {
    return this->push_back(x.data(), x.size());
}

inline void Node::push_back(const fastring& x) {
    return this->push_back(x.data(), x.size());
}

inline void Node::push_null() {
    return _root->_push_back(_root->_make_null(), _index);
}

inline Node Node::push_array() {
    return _root->_push_array(_index);
}

inline Node Node::push_object() {
    return _root->_push_object(_index);
}

inline Node Node::operator[](uint32 i) const {
    return _root->_at(i, _index);
}

inline Node Node::operator[](int i) const {
    return this->operator[]((uint32)i);
}

inline Node Node::operator[](Key key) const {
    return _root->_at(key, _index);
}

inline bool Node::has_member(Key key) const {
    return _root->_has_member(key, _index);
}

inline Node::iterator Node::begin() const {
    return _root->_begin(_index);
}

inline Node::iterator::iterator(Root* root, uint32 index, uint32 type)
    : _root(root), _index(index), _pos(0) {
    _step = (type == Root::kObject ? 2 : 1);
}

inline Node::Key Node::iterator::key() const {
    const uint32 index = ((xx::Array*)_root->_jb.at(_index))->p[_pos];
    return (char*)_root->_jb.at(index) + 8;
}

inline Node Node::iterator::value() const {
    const uint32 index = ((xx::Array*)_root->_jb.at(_index))->p[_pos + 1];
    return Node(_root, index);
}

inline Node Node::iterator::operator*() const {
    const uint32 index = ((xx::Array*)_root->_jb.at(_index))->p[_pos];
    return Node(_root, index);
}

inline Node::iterator& Node::iterator::operator++() {
    xx::Array* a = (xx::Array*) _root->_jb.at(_index);
    if ((_pos += _step) >= a->size) {
        _index = a->next;
        _pos = 0;
    }
    return *this;
}

inline bool Node::iterator::has_next() const {
    return _index != 0 && _pos < ((xx::Array*)_root->_jb.at(_index))->size;
}

inline bool Node::iterator::operator==(const End&) const {
    return !this->has_next();
}

inline bool Node::iterator::operator!=(const End&) const {
    return this->has_next();
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
    Root v;
    if (v.parse_from(s, n)) return std::move(v);
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

} // jsonv2
