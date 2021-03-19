#include "co/jsonv2.h"

namespace jsonv2 {

// json parser
//   @b: beginning of the string
//   @e: end of the string
// return the current position, or NULL on any error
class Parser {
  public:
    Parser(Root* root) : _root(root) {}
    ~Parser() = default;

    bool parse(const char* b, const char* e);
    const char* parse_object(const char* b, const char* e, uint32& index);
    const char* parse_array(const char* b, const char* e, uint32& index);
    const char* parse_string(const char* b, const char* e, uint32& index);
    const char* parse_unicode(const char* b, const char* e, fastream& s);
    const char* parse_number(const char* b, const char* e, uint32& index);
    const char* parse_key(const char* b, const char* e, uint32& index);
    const char* parse_false(const char* b, const char* e, uint32& index);
    const char* parse_true(const char* b, const char* e, uint32& index);
    const char* parse_null(const char* b, const char* e, uint32& index);
    const char* parse_value(const char* b, const char* e, uint32& index);

    inline bool is_white_char(char c) {
        return (c == ' ' || c == '\n' || c == '\r' || c == '\t');
    }

  private:
    Root* _root;
};

inline const char* Parser::parse_key(const char* b, const char* e, uint32& index) {
    if (*b++ != '"') return 0;
    const char* p = (const char*) memchr(b, '"', e - b);
    if (p) index = _root->_make_key(b, p - b);
    return p;
}

inline const char* Parser::parse_false(const char* b, const char* e, uint32& index) {
    if (e - b >= 5) {
        if (b[1] == 'a' && b[2] == 'l' && b[3] == 's' && b[4] == 'e') {
            index = _root->_make_bool(false);
            return b + 4;
        }
    }
    return 0;
}

inline const char* Parser::parse_true(const char* b, const char* e, uint32& index) {
    if (e - b >= 4) {
        if (b[1] == 'r' && b[2] == 'u' && b[3] == 'e') {
            index = _root->_make_bool(true);
            return b + 3;
        }
    }
    return 0;
}

inline const char* Parser::parse_null(const char* b, const char* e, uint32& index) {
    if (e - b >= 4) {
        if (b[1] == 'u' && b[2] == 'l' && b[3] == 'l') {
            index = _root->_make_null();
            return b + 3;
        }
    }
    return 0;
}

inline  const char* Parser::parse_value(const char* b, const char* e, uint32& index) {
    if (*b == '"') {
        return parse_string(b, e, index);
    } else if (*b == '{') {
        return parse_object(b, e, index);
    } else if (*b == '[') {
        return parse_array(b, e, index);
    } else if (*b == 'f') {
        return parse_false(b, e, index);
    } else if (*b == 't') {
        return parse_true(b, e, index);
    } else if (*b == 'n') {
        return parse_null(b, e, index);
    } else {
        return parse_number(b, e, index);
    }
}

inline bool Parser::parse(const char* b, const char* e) {
    while (b < e && is_white_char(*b)) ++b;
    if (b == e) return false;

    uint32 index;
    if (*b == '{') {
        b = parse_object(b, e, index);
    } else if (*b == '[') {
        b = parse_array(b, e, index);
    } else {
        b = parse_value(b, e, index);
    }

    if (b == 0) return false;
    while (++b < e && is_white_char(*b));
    return b == e;
}

const char* Parser::parse_object(const char* b, const char* e, uint32& index) {
    uint32 key, val;
    fastream& s = xx::jalloc()->alloc_stack();
    const size_t size = s.size();
    index = _root->_make_object();

    while (true) {
        while (++b < e && is_white_char(*b));
        if (b == e) goto err;
        if (*b == '}') goto end; // object end

        // key
        b = parse_key(b, e, key);
        if (b == 0) goto err;

        // ':'
        while (++b < e && is_white_char(*b));
        if (b == e || *b != ':') goto err;

        while (++b < e && is_white_char(*b));
        if (b == e) goto err;

        // value
        val = 0;
        b = parse_value(b, e, val);
        if (b == 0) goto err;

        s.append(key);
        s.append(val);
        //_root->_add_member(key, val, index);

        // check value end
        while (++b < e && is_white_char(*b));
        if (b == e) goto err;
        if (*b == '}') goto end; // object end
        if (*b != ',') goto err;
    }

  end:
    if (s.size() > size) {
        val = _root->_alloc_array(s.data() + size, s.size() - size);
        ((uint32*)_root->_jb.at(index))[1] = val;
        s.resize(size);
    }
    return b;
  err:
    s.clear();
    return 0;
}

const char* Parser::parse_array(const char* b, const char* e, uint32& index) {
    uint32 val;
    fastream& s = xx::jalloc()->alloc_stack();
    const size_t size = s.size();
    index = _root->_make_array();

    while (true) {
        while (++b < e && is_white_char(*b));
        if (b == e) goto err;
        if (*b == ']') goto end; // array end

        b = parse_value(b, e, val);
        if (b == 0) goto err;

        s.append(val);
        //_root->_push_back(v, index);

        while (++b < e && is_white_char(*b));
        if (b == e) goto err;
        if (*b == ']') goto end; // array end
        if (*b != ',') goto err;
    }

  end:
    if (s.size() > size) {
        val = _root->_alloc_array(s.data() + size, s.size() - size);
        ((uint32*)_root->_jb.at(index))[1] = val;
        s.resize(size);
    }
    return b;
  err:
    s.clear();
    return 0;
}

// find '"' or '\\'
inline const char* find_quote_or_escape(const char* b, const char* e) {
    const char* p = (const char*) memchr(b, '"', e - b);
    if (p == 0) return 0;
    const char* q = (const char*) memchr(b, '\\', p - b);
    return q ? q : p;
}

static inline const char* init_s2e_table() {
    static char tb[256] = { 0 };
    tb['r'] = '\r';
    tb['n'] = '\n';
    tb['t'] = '\t';
    tb['b'] = '\b';
    tb['f'] = '\f';
    tb['"'] = '"';
    tb['\\'] = '\\';
    tb['/'] = '/';
    tb['u'] = 'u';
    return tb;
}

const char* Parser::parse_string(const char* b, const char* e, uint32& index) {
    const char* p = find_quote_or_escape(++b, e); // find the first '"' or '\\'
    if (p == 0) return 0;
    if (*p == '"') {
        index = _root->_make_string(b, p - b);
        return p;
    }

    fastream& s = xx::jalloc()->alloc_stream();
    do {
        s.append(b, p - b);
        if (++p == e) return 0;

        static const char* tb = init_s2e_table();
        char c = tb[(uint8)*p];
        if (c == 0) return 0; // invalid escape

        if (*p != 'u') {
            s.append(c);
        } else {
            p = parse_unicode(p + 1, e, s);
            if (p == 0) return 0;
        }

        b = p + 1;
        p = find_quote_or_escape(b, e);
        if (p == 0) return 0;

        if (*p == '"') {
            s.append(b, p - b);
            index = _root->_make_string(s.data(), s.size());
            return p;
        }
    } while (true);
}

inline const char* parse_hex(const char* b, const char* e, uint32& u) {
    if (e - b < 4) return 0;
    for (int i = 0; i < 4; ++i) {
        u <<= 4;
        char c = b[i];
        if ('0' <= c && c <= '9') {
            u += c - '0';
        } else if ('A' <= c && c <= 'F') {
            u += c - 'A' + 10;
        } else if ('a' <= c && c <= 'f') {
            u += c - 'a' + 10;
        } else {
            return 0;
        }
    }
    return b + 3;
}

// utf8:
//   0000 - 007F      0xxxxxxx            
//   0080 - 07FF      110xxxxx  10xxxxxx        
//   0800 - FFFF      1110xxxx  10xxxxxx  10xxxxxx    
//  10000 - 10FFFF    11110xxx  10xxxxxx  10xxxxxx  10xxxxxx
//
// \uXXXX
// \uXXXX\uYYYY
//   D800 <= XXXX <= DBFF
//   DC00 <= XXXX <= DFFF
const char* Parser::parse_unicode(const char* b, const char* e, fastream& s) {
    uint32 u = 0;
    b = parse_hex(b, e, u);
    if (b == 0) return 0;

    if (0xD800 <= u && u <= 0xDBFF) {
        if (e - b < 3) return 0;
        if (b[1] != '\\' || b[2] != 'u') return 0;

        uint32 v = 0;
        b = parse_hex(b + 3, e, v);
        if (b == 0) return 0;
        if (v < 0xDC00 || v > 0xDFFF) return 0;

        u = 0x10000 + (((u - 0xD800) << 10) | (v - 0xDC00));
    }

    // encode to UTF8
    if (u <= 0x7F) {
        s.append((char)u);
    } else if (u <= 0x7FF) {
        s.append((char) (0xC0 | (0xFF & (u >> 6))));
        s.append((char) (0x80 | (0x3F & u)));
    } else if (u <= 0xFFFF) {
        s.append((char) (0xE0 | (0xFF & (u >> 12))));
        s.append((char) (0x80 | (0x3F & (u >> 6))));
        s.append((char) (0x80 | (0x3F & u)));
    } else {
        assert(u <= 0x10FFFF);
        s.append((char) (0xF0 | (0xFF & (u >> 18))));
        s.append((char) (0x80 | (0x3F & (u >> 12))));
        s.append((char) (0x80 | (0x3F & (u >>  6))));
        s.append((char) (0x80 | (0x3F & u)));
    } 

    return b;
}

inline int64 str2int(const char* b, const char* e) {
    uint64 v = 0;
    const char* p = b;
    if (*p == '-') ++p;
    for (; p < e; ++p) v = v * 10 + *p - '0';
    return *b != '-' ? v : -(int64)v;
}

inline bool str2double(const char* b, double& d) {
    errno = 0;
    d = strtod(b, 0);
    return !(errno == ERANGE && (d == HUGE_VAL || d == -HUGE_VAL));
}

inline bool is_digit(char c) {
    return '0' <= c && c <= '9';
}

inline const char* find_non_digit(const char* b, const char* e) {
    for (const char* p = b; p < e; ++p) {
        if (!is_digit(*p)) return p;
    }
    return e;
}

const char* Parser::parse_number(const char* b, const char* e, uint32& index) {
    bool is_double = false;
    const char* p = b;

    if (*p == '-' && ++p == e) return 0;

    if (*p == '0') {
        if (++p == e) goto digit_end;
    } else {
        if (*p < '1' || *p > '9') return 0; // must be 1 to 9
        if ((p = find_non_digit(p + 1, e)) == e) goto digit_end;
    }

    if (*p == '.') {
        ++p;
        if (p == e || !is_digit(*p)) return 0; // must be a digit after the point
        is_double = true;
        if ((p = find_non_digit(p + 1, e)) == e) goto digit_end;
    }

    if (*p == 'e' || *p == 'E') {
        if (++p == e) return 0;
        if (*p == '-' || *p == '+') ++p;
        if (p == e || !is_digit(*p)) return 0; // must be a digit
        is_double = true;
        p = find_non_digit(p + 1, e);
    }

  digit_end:
    {
        size_t n = p - b;
        if (n == 0) return 0;

        if (is_double || n > 20) goto to_dbl;
        if (n < 20) goto to_int;
    }
    {
        // compare with MAX_UINT64, MIN_INT64
        // if value > MAX_UINT64 or value < MIN_INT64, we parse it as a double
        int m = memcmp(b, (*b != '-' ? "18446744073709551615" : "-9223372036854775808"), 20);
        if (m < 0) goto to_int;
        if (m > 0) goto to_dbl;
        index = _root->_make_int(*b != '-' ? MAX_UINT64 : MIN_INT64);
        return p - 1;
    }

  to_int:
    index = _root->_make_int(str2int(b, p));
    return p - 1;

  to_dbl:
    double d;
    if (p == e && *p != '\0') {
        fastream& fs = xx::jalloc()->alloc_stream();
        fs.append(b, p - b);
        b = fs.c_str();
    }
    if (str2double(b, d)) {
        index = _root->_make_double(d);
        return p - 1;
    } else {
        return 0;
    }
}

bool Root::parse_from(const char* s, size_t n) {
    if (_mem == 0) {
        _mem = xx::jalloc()->alloc_jblock(_b8(n));
    } else {
        _jb.reserve(_b8(n));
    }
    Parser parser(this);
    return parser.parse(s, s + n);
}

void Root::_add_member(uint32 key, uint32 val, uint32 index) {
    if (this->is_null()) {
        _Header* h = (_Header*)_jb.at(0);
        h->type = kObject;
        h->index = 0;
    }

    _Header* h = (_Header*)_jb.at(index);
    assert(h->type == kObject);

    if (h->index != 0) {
        for (uint32 i = h->index;;) {
            xx::Array* a = (xx::Array*) _jb.at(i);
            if (!a->next) {
                if (!a->full()) {
                    a->push(key, val);
                } else {
                    uint32 k = this->_alloc_array(16);
                    ((xx::Array*)_jb.at(i))->next = k;
                    ((xx::Array*)_jb.at(k))->push(key, val);
                }
                break;
            }
            i = a->next;
        }

    } else {
        uint32 i = this->_alloc_array(16);
        ((_Header*)_jb.at(index))->index = i;
        ((xx::Array*)_jb.at(i))->push(key, val);
    }
}

void Root::_push_back(uint32 val, uint32 index) {
    if (this->is_null()) {
        _Header* h = (_Header*)_jb.at(0);
        h->type = kArray;
        h->index = 0;
    }

    _Header* h = (_Header*)_jb.at(index);
    assert(h->type == kArray);

    if (h->index != 0) {
        for (uint32 i = h->index;;) {
            xx::Array* a = (xx::Array*) _jb.at(i);
            if (!a->next) {
                if (!a->full()) {
                    a->push(val);
                } else {
                    uint32 k = this->_alloc_array(8);
                    ((xx::Array*)_jb.at(i))->next = k;
                    ((xx::Array*)_jb.at(k))->push(val);
                }
                break;
            }
            i = a->next;
        }

    } else {
        uint32 i = this->_alloc_array(8);
        ((_Header*)_jb.at(index))->index = i;
        ((xx::Array*)_jb.at(i))->push(val);
    }
}

Node Root::_at(uint32 i, uint32 index) const {
    assert(_mem);
    _Header* h = (_Header*) _jb.at(index);
    assert(h->type == kArray);

    for (uint32 k = h->index;;) {
        assert(k != 0);
        xx::Array* a = (xx::Array*) _jb.at(k);
        if (i < a->size) return Node((Root*)this, a->p[i]);
        i -= a->size;
        k = a->next;
    }
}

Node Root::_at(Key key, uint32 index) const {
    if (_mem == 0) new ((Root*)this) Root(TypeObject());
    _Header* h = (_Header*) _jb.at(index);
    assert(h->type == kObject);

    for (auto it = this->_begin(index); it != this->end(); ++it) {
        if (strcmp(it.key(), key) == 0) return it.value();
    }

    const uint32 k = ((Root*)this)->_make_key(key);
    const uint32 v = ((Root*)this)->_make_null();
    ((Root*)this)->_add_member(k, v, index);
    return Node((Root*)this, v);
}

bool Root::_has_member(Key key, uint32 index) const {
    if (_mem == 0) return false;
    _Header* h = (_Header*) _jb.at(index);
    if (h->type == kNull) return false;
    assert(h->type == kObject);

    for (auto it = this->_begin(index); it != this->end(); ++it) {
        if (strcmp(it.key(), key) == 0) return true;
    }
    return false;
}
} // json
