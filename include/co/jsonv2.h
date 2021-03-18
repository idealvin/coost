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
        _h = (_Header*) malloc(sizeof(_Header) + 8 * cap);
        _h->cap = (uint32)cap;
        _h->size = 0;
        CLOG << "jblock cap: " << cap;
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
    JAlloc() = default;

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

  private:
    std::vector<void*> _jb;
    fastream _fs; // for parsing string
};

inline JAlloc* jalloc() {
    static __thread JAlloc* alloc = 0;
    return alloc ? alloc : (alloc = new JAlloc);
}

struct Piece {
    Piece() : size(0), next(0) {}
    uint32 size;
    uint32 next;
    uint32 e[];
};

template<int N>
class Array {
  public:
    Array()
        : _cap(0), _size(0), _tail(0), _next(0) {
    }
    ~Array() = default;

    typedef uint32 T;

    void push(T x) {
        Piece* p;
        if (++_size <= _cap) {
            p = piece(_tail);
        } else {
            void* b = jblock();
            JBlock& jb = *(JBlock*)&b;
            uint32 index = jb.alloc((8 + N * sizeof(T)) >> 3);
            p = new (jb.at(index)) Piece();
            _cap += N;
            if (_tail) ((Piece*)jb.at(_tail))->next = index;
            _tail = index;
        }
        p->e[p->size++] = x;
    }

    void push(T x, T y) {
        Piece* p;
        if ((_size += 2) <= _cap) {
            p = piece(_tail);
        } else {
            void* b = jblock();
            JBlock& jb = *(JBlock*)&b;
            uint32 index = jb.alloc((8 + N * sizeof(T)) >> 3);
            p = new (jb.at(index)) Piece();
            _cap += N;
            if (_tail) ((Piece*)jb.at(_tail))->next = index;
            _tail = index;
        }
        p->e[p->size++] = x;
        p->e[p->size++] = y;
    }

    T& operator[](uint32 n) {
        void* b = jblock();
        JBlock& jb = *(JBlock*)&b;
        if (n < N) return ((Piece*)jb.at(_next))->e[n];

        uint32 r = n & (N - 1); // i % N
        uint32 q = n / N;
        uint32 index = _next;
        for (uint32 i = 0; i < q; ++i) {
            index = ((Piece*)jb.at(index))->next;
        }
        return ((Piece*)jb.at(index))->e[r];
    }

    uint32 size() const {
        return _size;
    }

  private:
    uint32 _cap;
    uint32 _size;
    uint32 _tail;
    uint32 _next;

    Piece* piece(uint32 index) {
        return (Piece*) ((uint64*)this - ((uint32*)this)[-1] - 1 + index);
    }

    uint64* jblock() {
        return (uint64*)this - ((uint32*)this)[-1] - 2;
    }
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
    struct TypeArray {};
    struct TypeObject {};
    typedef const char* Key;

    Root() : _mem(0) {}

    ~Root() {
        if (_mem) xx::jalloc()->dealloc_jblock(_mem);
    }

    Root(TypeArray) : _mem(xx::jalloc()->alloc_jblock()) {
        _make_array();
    }

    Root(TypeObject) : _mem(xx::jalloc()->alloc_jblock()) {
        _make_object();
    }

    void add_member(Key key, bool x) {
        return this->_add_member(key, x, 0);
    }

    void add_member(Key key, int64 x) {
        return this->_add_member(key, x, 0);
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
        return this->_add_member(key, x, 0);
    }

    void add_member(Key key, const char* x, size_t n) {
        return this->_add_member(key, x, n, 0);
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
        return this->_add_null(key, 0);
    }

    Node add_array(Key key) {
        return this->_add_array(key, 0);
    }

    Node add_object(Key key) {
        return this->_add_object(key, 0);
    }

    void push_back(bool x) {
        return this->_push_back(x, 0);
    }

    void push_back(int64 x) {
        return this->_push_back(x, 0);
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
        return this->_push_back(x, 0);
    }

    void push_back(const char* x, size_t n) {
        return this->_push_back(x, n, 0);
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
        return this->_push_null(0);
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

  private:
    void _Json2str(fastream& fs, bool debug=false) const;
    void _Json2pretty(fastream& fs, int indent, int n) const;

    uint32 _b8(uint32 n) {
        return (uint32)((n >> 3) + !!(n & 7));
    }

    // | type: 4 | index: 4 | reserved: 8 |
    uint32 _make_null() {
        uint32 index = _jb.alloc(2);
        _Header* h = (_Header*)_jb.at(index);
        h->type = kNull;
        h->index = index;
        return index;
    }

    // | type: 4 | index: 4 | val: 8 |
    uint32 _make_bool(bool x) {
        uint32 index = _jb.alloc(2);
        _Header* h = (_Header*)_jb.at(index);
        h->type = kBool;
        h->index = index;
        h->b = x;
        return index;
    }

    // | type: 4 | index: 4 | val: 8 |
    uint32 _make_int(int64 x) {
        uint32 index = _jb.alloc(2);
        _Header* h = (_Header*)_jb.at(index);
        h->type = kInt;
        h->index = index;
        h->i = x;
        return index;
    }

    // | type: 4 | index: 4 | val: 8 |
    uint32 _make_double(double x) {
        uint32 index = _jb.alloc(2);
        _Header* h = (_Header*)_jb.at(index);
        h->type = kDouble;
        h->index = index;
        h->d = x;
        return index;
    }

    // | type: 4 | index: 4 | header: 8 | body |
    uint32 _make_string(const char* s, size_t n) {
        uint32 index = _jb.alloc(_b8(n + 1 + 8 + 8));
        _Header* h = (_Header*)_jb.at(index);
        h->type = kString;
        h->index = index;
        h->slen = n;
        uint32* p = (uint32*)h + 4;
        memcpy(p, s, n);
        p[n] = '\0';
        return index;
    }

    // | 4 | length: 4 |
    uint32 _make_key(const char* x, size_t n) {
        uint32 index = _jb.alloc(_b8(n + 1 + 8));
        uint32* s = (uint32*)_jb.at(index) + 2;
        s[-1] = n;
        memcpy(s, x, n);
        s[n] = '\0';
        return index;
    }

    uint32 _make_key(Key key) {
        const uint32 len = strlen(key);
        uint32 index = _jb.alloc(_b8(len + 1 + 8));
        uint32* s = (uint32*)_jb.at(index) + 2;
        s[-1] = len;
        memcpy(s, key, len + 1);
        return index;
    }

    // |type: 4|index: 4|Array<8>: 16| + Piece:|size:4|next:4|body|...
    uint32 _make_array() {
        uint32 index = _jb.alloc(3);
        _Header* h = (_Header*)_jb.at(index);
        h->type = kArray;
        h->index = index;
        new (&h->a) xx::Array<8>();
        return index;
    }

    // |type: 4|index: 4|Array<16>: 16| + Piece:|size:4|next:4|body|...
    uint32 _make_object() {
        uint32 index = _jb.alloc(3);
        _Header* h = (_Header*)_jb.at(index);
        h->type = kObject;
        h->index = index;
        new (&h->o) xx::Array<16>();
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
        return (char*)h + 16;
    }

    void _add_member(uint32 key, uint32 val, uint32 index) {
        if (_mem == 0) new (this) Root(TypeObject());
        _Header* h = (_Header*)_jb.at(index);
        assert(h->type == kObject);
        h->o.push(key, val);
    }

    void _add_member(Key key, bool x, uint32 index) {
        return this->_add_member(_make_key(key), _make_bool(x), index);
    }

    void _add_member(Key key, int64 x, uint32 index) {
        return this->_add_member(_make_key(key), _make_int(x), index);
    }

    void _add_member(Key key, double x, uint32 index) {
        return this->_add_member(_make_key(key), _make_double(x), index);
    }

    void _add_member(Key key, const char* x, size_t n, uint32 index) {
        return this->_add_member(_make_key(key), _make_string(x, n), index);
    }

    void _add_null(Key key, uint32 index) {
        return this->_add_member(_make_key(key), _make_null(), index);
    }

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

    void _push_back(uint32 val, uint32 index) {
        if (_mem == 0) new (this) Root(TypeArray());
        _Header* h = (_Header*)_jb.at(index);
        assert(h->type == kArray);
        h->a.push(val);
    }

    void _push_back(bool x, uint32 index) {
        return this->_push_back(_make_bool(x), index);
    }

    void _push_back(int64 x, uint32 index) {
        return this->_push_back(_make_int(x), index);
    }

    void _push_back(double x, uint32 index) {
        return this->_push_back(_make_double(x), index);
    }

    void _push_back(const char* x, size_t n, uint32 index) {
        return this->_push_back(_make_string(x, n), index);
    }

    void _push_null(uint32 index) {
        return this->_push_back(_make_null(), index);
    }

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
        uint32 index;
        union {
            bool b;          // for bool
            int64 i;         // for int
            double d;        // for double
            uint32 slen;     // for string length
            xx::Array<8> a;  // for array
            xx::Array<16> o; // for object
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
    return _root->_add_member(key, x, _index);
}

inline void Node::add_member(Key key, int64 x) {
    return _root->_add_member(key, x, _index);
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
    return _root->_add_member(key, x, _index);
}

inline void Node::add_member(Key key, const char* x, size_t n) {
    return _root->_add_member(key, x, n, _index);
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
    return _root->_add_null(key, _index);
}

inline Node Node::add_array(Key key) {
    return _root->_add_array(key, _index);
}

inline Node Node::add_object(Key key) {
    return _root->_add_object(key, _index);
}

inline void Node::push_back(bool x) {
    return _root->_push_back(x, _index);
}

inline void Node::push_back(int64 x) {
    return _root->_push_back(x, _index);
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
    return _root->_push_back(x, _index);
}

inline void Node::push_back(const char* x, size_t n) {
    return _root->_push_back(x, n, _index);
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
    return _root->_push_null(_index);
}

inline Node Node::push_array() {
    return _root->_push_array(_index);
}

inline Node Node::push_object() {
    return _root->_push_object(_index);
}
} // jsonv2
