#include "co/json.h"
#ifdef CO_SSE42
#include <nmmintrin.h>
#endif

namespace json {

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

inline bool is_white_space(char c) {
    return (c == ' ' || c == '\n' || c == '\r' || c == '\t');
}

inline bool Parser::parse(const char* b, const char* e) {
    while (b < e && is_white_space(*b)) ++b;
    if (b >= e) return false;

    uint32 index;
    if (*b == '{') {
        b = parse_object(b, e, index);
    } else if (*b == '[') {
        b = parse_array(b, e, index);
    } else {
        b = parse_value(b, e, index);
        //return false; // Root must be an array or object
    }

    if (b == 0) return false;
    while (++b < e && is_white_space(*b));
    return b == e;
}

const char* Parser::parse_object(const char* b, const char* e, uint32& index) {
    uint32 key, val;
    fastream& s = xx::jalloc()->alloc_stack();
    const size_t size = s.size();
    index = _root->_make_object();

    while (true) {
        while (++b < e && is_white_space(*b));
        if (b == e) goto err;
        if (*b == '}') goto end; // object end

        b = parse_key(b, e, key);
        if (b == 0) goto err;

        while (++b < e && is_white_space(*b));
        if (b == e || *b != ':') goto err;

        while (++b < e && is_white_space(*b));
        if (b == e) goto err;

        b = parse_value(b, e, val);
        if (b == 0) goto err;

        s.append(key);
        s.append(val);

        while (++b < e && is_white_space(*b));
        if (b == e) goto err;
        if (*b == '}') goto end; // object end
        if (*b != ',') goto err;
    }

  end:
    if (s.size() > size) {
        val = _root->_alloc_queue(s.data() + size, s.size() - size);
        ((Root::_Header*)_root->_p8(index))->index = val;
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
        while (++b < e && is_white_space(*b));
        if (b == e) goto err;
        if (*b == ']') goto end; // array end

        b = parse_value(b, e, val);
        if (b == 0) goto err;

        s.append(val);

        while (++b < e && is_white_space(*b));
        if (b == e) goto err;
        if (*b == ']') goto end; // array end
        if (*b != ',') goto err;
    }

  end:
    if (s.size() > size) {
        val = _root->_alloc_queue(s.data() + size, s.size() - size);
        ((Root::_Header*)_root->_p8(index))->index = val;
        s.resize(size);
    }
    return b;
  err:
    s.clear();
    return 0;
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

inline const char* find_quote    (const char* b, const char* e) { return (const char*) memchr(b, '"', e - b); }
inline const char* find_backslash(const char* b, const char* e) { return (const char*) memchr(b, '\\', e - b); }

const char* Parser::parse_string(const char* b, const char* e, uint32& index) {
    const char *p, *q;
    if ((p = find_quote(++b, e)) == 0) return 0;
    q = find_backslash(b, p);
    if (q == 0) {
        index = _root->_make_string(b, p - b);
        return p;
    }

    fastream& s = xx::jalloc()->alloc_stream();
    do {
        s.append(b, q - b);
        if (++q == e) return 0;

        static const char* tb = init_s2e_table();
        char c = tb[(uint8)*q];
        if (c == 0) return 0; // invalid escape

        if (*q != 'u') {
            s.append(c);
        } else {
            q = parse_unicode(q + 1, e, s);
            if (q == 0) return 0;
        }

        b = q + 1;
        if (b > p) {
            p = find_quote(b, e);
            if (p == 0) return 0;
        }
        
        q = find_backslash(b, p);
        if (q == 0) {
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

const char* Parser::parse_number(const char* b, const char* e, uint32& index) {
    bool is_double = false;
    const char* p = b;

    if (*p == '-' && ++p == e) return 0;

    if (*p == '0') {
        if (++p == e) goto digit_end;
    } else {
        if (*p < '1' || *p > '9') return 0; // must be 1 to 9
        while (++p < e && is_digit(*p));
        if (p == e) goto digit_end;
    }

    if (*p == '.') {
        if (++p == e || !is_digit(*p)) return 0; // must be a digit after the point
        is_double = true;
        while (++p < e && is_digit(*p));
        if (p == e) goto digit_end;
    }

    if (*p == 'e' || *p == 'E') {
        if (++p == e) return 0;
        if (*p == '-' || *p == '+') ++p;
        if (p == e || !is_digit(*p)) return 0; // must be a digit
        is_double = true;
        while (++p < e && is_digit(*p));
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
    }
    return 0;
}

bool Root::parse_from(const char* s, size_t n) {
    if (_mem == 0) {
        _mem = xx::jalloc()->alloc_jblock(_b8(n + (n >> 1)));
    } else {
        _jb.clear();
        _jb.reserve(_b8(n + (n >> 1)));
    }
    Parser parser(this);
    return parser.parse(s, s + n);
}

static inline const char* init_e2s_table() {
    static char tb[256] = { 0 };
    tb['\r'] = 'r';
    tb['\n'] = 'n';
    tb['\t'] = 't';
    tb['\b'] = 'b';
    tb['\f'] = 'f';
    tb['\"'] = '"';
    tb['\\'] = '\\';
    return tb;
}

#ifdef CO_SSE42 
static const char* find_escapse(const char* b, const char* e, char& c) {
    static const char* tb = init_e2s_table();
    static const char esc[16] = "\r\n\t\b\f\"\\";
    static const __m128i w = _mm_loadu_si128((const __m128i*)esc);

    const char* p = b;
    const char* b16 = (const char*)(((size_t)b + 15) & ~(size_t)15);
    if (b16 >= e) goto tail;

    for (; p != b16; ++p) {
        if ((c = tb[(uint8)*p])) return p;
    }

    for (; p + 16 <= e; p += 16) {
        const __m128i s = _mm_load_si128((const __m128i*)p);
        int r = _mm_cmpistri(w, s, _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY);
        if (r < 16) {
            c = tb[(uint8)*(p + r)];
            return p + r;
        }
    }

  tail:
    for (; p < e; ++p) {
        if ((c = tb[(uint8)*p])) return p;
    }
    return e;
}
#else
inline const char* find_escapse(const char* b, const char* e, char& c) {
    static const char* tb = init_e2s_table();
    for (const char* p = b; p < e; ++p) {
        if ((c = tb[(uint8)*p])) return p;
    }
    return e;
}
#endif

fastream& Root::_Json2str(fastream& fs, bool debug, uint32 index) const {
    _Header* h = (_Header*) _p8(index);
    if (h->type == kString) {
        fs << '"';
        const uint32 len = h->size;
        const bool trunc = debug && len > 256;
        const char* s = (const char*) _p8(h->index);
        const char* e = trunc ? s + 256 : s + len;

        char c;
        for (const char* p; (p = find_escapse(s, e, c)) < e;) {
            fs.append(s, p - s).append('\\').append(c);
            s = p + 1;
        }

        if (s != e) fs.append(s, e - s);
        if (trunc) fs.append(3, '.');
        fs << '"';

    } else if (h->type == kObject) {
        fs << '{';
        for (uint32 k = h->index; k != 0;) {
            xx::Queue* a = (xx::Queue*) _p8(k);
            for (uint32 i = 0; i < a->size; i += 2) {
                fs << '"' << (const char*)_p8(a->p[i]) << '"' << ':'; // key
                _Json2str(fs, debug, a->p[i + 1]) << ',';             // value
            }
            k = a->next;
        }
        if (fs.back() == ',') fs.back() = '}';
        else fs << '}';

    } else if (h->type == kArray) {
        fs << '[';
        for (uint32 k = h->index; k != 0;) {
            xx::Queue* a = (xx::Queue*) _p8(k);
            for (uint32 i = 0; i < a->size; ++i) {
                _Json2str(fs, debug, a->p[i]) << ',';
            }
            k = a->next;
        }
        if (fs.back() == ',') fs.back() = ']';
        else fs << ']';

    } else {
        if (h->type == kInt) {
            fs << h->i;
        } else if (h->type == kBool) {
            fs << h->b;
        } else if (h->type == kDouble) {
            fs << h->d;
        } else {
            assert(h->type == kNull);
            fs << "null";
        }
    }
    return fs;
}

// @indent:  4 spaces by default
// @n:       number of spaces to insert at the beginning for the current line
fastream& Root::_Json2pretty(fastream& fs, int indent, int n, uint32 index) const {
    _Header* h = (_Header*) _p8(index);
    if (h->type == kObject) {
        fs << '{';
        for (uint32 k = h->index; k != 0;) {
            xx::Queue* a = (xx::Queue*) _p8(k);
            for (uint32 i = 0; i < a->size; i += 2) {
                fs.append('\n').append(n, ' ');
                fs << '"' << (const char*)_p8(a->p[i]) << '"' << ": ";    // key
                _Json2pretty(fs, indent, n + indent, a->p[i + 1]) << ','; // value
            }
            k = a->next;
        }
        if (fs.back() == ',') fs.back() = '\n';
        if (n > indent) fs.append(n - indent, ' ');
        fs << '}';

    } else if (h->type == kArray) {
        fs << '[';
        for (uint32 k = h->index; k != 0;) {
            xx::Queue* a = (xx::Queue*) _p8(k);
            for (uint32 i = 0; i < a->size; ++i) {
                fs.append('\n').append(n, ' ');
                _Json2pretty(fs, indent, n + indent, a->p[i]) << ',';
            }
            k = a->next;
        }
        if (fs.back() == ',') fs.back() = '\n';
        if (n > indent) fs.append(n - indent, ' ');
        fs << ']';

    } else {
        _Json2str(fs, false, index);
    }
    return fs;
}

void Root::_add_member(uint32 key, uint32 val, uint32 index) {
    _Header* h = (_Header*)_p8(index);
    if (h->type != kNull) {
        assert(h->type == kObject);
        if (h->index == 0) goto empty;

        for (uint32 q = h->index;;) {
            xx::Queue* a = (xx::Queue*) _p8(q);
            if (!a->next) {
                if (!a->full()) {
                    a->push(key, val);
                } else {
                    uint32 k = this->_alloc_queue(16);
                    ((xx::Queue*)_p8(q))->next = k;
                    ((xx::Queue*)_p8(k))->push(key, val);
                }
                return;
            }
            q = a->next;
        }
    } else {
        h->type = kObject;
    }
    
  empty:
    {
        uint32 q = this->_alloc_queue(16);
        ((_Header*)_p8(index))->index = q;
        ((xx::Queue*)_p8(q))->push(key, val);
    }
}

void Root::_push_back(uint32 val, uint32 index) {
    _Header* h = (_Header*)_p8(index);
    if (h->type != kNull) {
        assert(h->type == kArray);
        if (h->index == 0) goto empty;

        for (uint32 q = h->index;;) {
            xx::Queue* a = (xx::Queue*) _p8(q);
            if (!a->next) {
                if (!a->full()) {
                    a->push(val);
                } else {
                    uint32 k = this->_alloc_queue(8);
                    ((xx::Queue*)_p8(q))->next = k;
                    ((xx::Queue*)_p8(k))->push(val);
                }
                return;
            }
            q = a->next;
        }
    } else {
        h->type = kArray;
    }

  empty:
    {
        uint32 q = this->_alloc_queue(8);
        ((_Header*)_p8(index))->index = q;
        ((xx::Queue*)_p8(q))->push(val);
    }
}

Value Root::_at(uint32 i, uint32 index) const {
    _Header* h = (_Header*) _p8(index);
    assert(h->type == kArray);

    for (uint32 k = h->index;;) {
        assert(k != 0);
        xx::Queue* a = (xx::Queue*) _p8(k);
        if (i < a->size) return Value((Root*)this, a->p[i]);
        i -= a->size;
        k = a->next;
    }
}

Value Root::_at(Key key, uint32 index) const {
    _Header* h = (_Header*) _p8(index);
    assert(h->type == kObject);

    for (uint32 k = h->index; k != 0;) {
        xx::Queue* a = (xx::Queue*) _p8(k);
        for (uint32 i = 0; i < a->size; i += 2) {
            if (strcmp((const char*)_p8(a->p[i]), key) == 0) {
                return Value((Root*)this, a->p[i + 1]);
            }
        }
        k = a->next;
    }

    const uint32 k = ((Root*)this)->_make_key(key);
    const uint32 v = ((Root*)this)->_make_null();
    ((Root*)this)->_add_member(k, v, index);
    return Value((Root*)this, v);
}

bool Root::_has_member(Key key, uint32 index) const {
    _Header* h = (_Header*) _p8(index);
    if (h->type == kNull) return false;
    assert(h->type == kObject);

    for (uint32 k = h->index; k != 0;) {
        xx::Queue* a = (xx::Queue*) _p8(k);
        for (uint32 i = 0; i < a->size; i += 2) {
            if (strcmp((const char*)_p8(a->p[i]), key) == 0) {
                return true;
            }
        }
        k = a->next;
    }
    return false;
}

uint32 Root::_size(uint32 index) const {
    _Header* h = (_Header*) _p8(index);
    if (h->type == kString) return h->size;
    if (h->type & (kObject | kArray)) {
        uint32 n = 0;
        for (uint32 k = h->index; k != 0;) {
            xx::Queue* a = (xx::Queue*) _p8(k);
            n += a->size;
            k = a->next;
        }
        return h->type == kObject ? (n >> 1) : n;
    }
    return 0;
}

} // json
