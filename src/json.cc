#include "co/json.h"
#include "co/log.h"
#include <errno.h>
#include <math.h>
#ifdef CO_SSE42
#include <nmmintrin.h>
#endif

namespace json {

Value& Value::operator[](Key key) {
    if (this->is_null()) new (this) Value(JObject());
    for (auto it = this->begin(); it != this->end(); ++it) {
        if (strcmp(it->key, key) == 0) return it->value;
    }

    _p->o.push_back(_Alloc_key(key));
    _p->o.push_back(0); // null value 
    return *(Value*) &_p->o.back();
}

const Value& Value::operator[](Key key) const {
    if (!this->is_null()) {
        for (auto it = this->begin(); it != this->end(); ++it) {
            if (strcmp(it->key, key) == 0) return it->value;
        }
    }
    return Jalloc::instance()->alloc_null();
}

bool Value::has_member(Key key) const {
    if (this->is_object()) {
        for (auto it = this->begin(); it != this->end(); ++it) {
            if (strcmp(it->key, key) == 0) return true;
        }
    }
    return false;
}

void Value::_UnRef() {
    static_assert(offsetof(struct _Mem, a) == 8, "");
    if (atomic_dec(&_p->refn) == 0) {
        if (_p->type == kObject) {
            Array& o = _p->o;
            for (uint32 i = 0; i < o.size(); i += 2) {
                Jalloc::instance()->dealloc_string(o[i]);
                ((Value*)&o[i + 1])->~Value();
            }
            o.~Array();
        } else if (_p->type == kArray) {
            Array& a = _p->a;
            for (uint32 i = 0; i < a.size(); ++i) {
                ((Value*)&a[i])->~Value();
            }
            a.~Array();
        } else if (_p->type == kString) {
            Jalloc::instance()->dealloc_string(_p->s);
        }
        Jalloc::instance()->dealloc_mem(_p);
    }
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

#ifdef SSE42 
const char* find_escapse(const char* b, const char* e, char& c) {
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

void Value::_Json2str(fastream& fs, bool debug) const {
    if (this->is_null()) {
        fs << "null";
        return;
    }

    if (_p->type == kString) {
        fs << '"';
        const uint32 len = _p->l[-1];
        const bool trunc = debug && len > 256;
        const char* s = _p->s;
        const char* e = trunc ? s + 256 : s + len;

        char c;
        for (const char* p; (p = find_escapse(s, e, c)) < e;) {
            fs.append(s, p - s).append('\\').append(c);
            s = p + 1;
        }

        if (s != e) fs.append(s, e - s);
        if (trunc) fs.append(3, '.');
        fs << '"';

    } else if (_p->type == kObject) {
        fs << '{';
        auto it = this->begin();
        if (it != this->end()) {
            fs << '"' << it->key << '"' << ':';
            it->value._Json2str(fs, debug);
            for (; ++it != this->end();) {
                fs << ',' << '"' << it->key << '"' << ':';
                it->value._Json2str(fs, debug);
            }
        }
        fs << '}';

    } else if (_p->type == kArray) {
        fs << '[';
        Array& a = _p->a;
        if (!a.empty()) {
            ((Value*)&a[0])->_Json2str(fs, debug);
            for (uint32 i = 1; i < a.size(); ++i) {
                fs << ',';
                ((Value*)&a[i])->_Json2str(fs, debug);
            }
        }
        fs << ']';

    } else {
        if (_p->type == kInt) {
            fs << _p->i;
        } else if (_p->type == kBool) {
            fs << _p->b;
        } else {
            assert(_p->type == kDouble);
            fs << _p->d;
        }
    }
}

// @indent:  4 spaces by default
// @n:       number of spaces to insert at the beginning for the current line
void Value::_Json2pretty(fastream& fs, int indent, int n) const {
    if (this->is_null()) {
        fs << "null";
        return;
    }

    if (_p->type == kObject) {
        fs << '{';
        auto it = this->begin();
        if (it != this->end()) {
            for (;;) {
                fs.append('\n').append(n, ' ');
                fs << '"' << it->key << '"' << ": ";

                Value& v = it->value;
                if (v.is_object() || v.is_array()) {
                    v._Json2pretty(fs, indent, n + indent);
                } else {
                    v._Json2str(fs);
                }

                if (++it != this->end()) {
                    fs << ',';
                } else {
                    fs << '\n';
                    break;
                }
            }
        }

        if (n > indent) fs.append(n - indent, ' ');
        fs << '}';

    } else if (_p->type == kArray) {
        fs << '[';
        Array& a = _p->a;
        if (!a.empty()) {
            for (uint32 i = 0;;) {
                fs.append('\n').append(n, ' ');

                Value& v = *(Value*) &a[i];
                if (v.is_object() || v.is_array()) {
                    v._Json2pretty(fs, indent, n + indent);
                } else {
                    v._Json2str(fs);
                }

                if (++i < a.size()) {
                    fs << ',';
                } else {
                    fs << '\n';
                    break;
                }
            }
        }

        if (n > indent) fs.append(n - indent, ' ');
        fs << ']';

    } else {
        _Json2str(fs);
    }
}

// json parser
//   @b: beginning of the string
//   @e: end of the string
//   @s: stack for parsing string type
//   @r: result
// return the current position, or NULL on any error
static const char* parse_object(const char* b, const char* e, Value* r);
static const char* parse_array(const char* b, const char* e, Value* r);
static const char* parse_string(const char* b, const char* e, Value* r);
static const char* parse_unicode(const char* b, const char* e, fastream& s);
static const char* parse_number(const char* b, const char* e, Value* r);

inline bool is_white_char(char c) {
    return (c == ' ' || c == '\n' || c == '\r' || c == '\t');
}

inline const char* parse_key(const char* b, const char* e, char** key) {
    if (*b++ != '"') return 0;
    const char* p = (const char*) memchr(b, '"', e - b);
    if (p) {
        const uint32 len = (uint32)(p - b);
        char* s = Value::Jalloc::instance()->alloc_string(len + 1);
        memcpy(s, b, len);
        s[len] = '\0';
        *key = s;
    }
    return p;
}

inline const char* parse_false(const char* b, const char* e, Value* r) {
    if (e - b < 5) return 0;
    if (b[1] != 'a' || b[2] != 'l' || b[3] != 's' || b[4] != 'e') return 0;
    new (r) Value(false);
    return b + 4;
}

inline const char* parse_true(const char* b, const char* e, Value* r) {
    if (e - b < 4) return 0;
    if (b[1] != 'r' || b[2] != 'u' || b[3] != 'e') return 0;
    new (r) Value(true);
    return b + 3;
}

inline const char* parse_null(const char* b, const char* e, Value* r) {
    if (e - b < 4) return 0;
    if (b[1] != 'u' || b[2] != 'l' || b[3] != 'l') return 0;
    return b + 3;
}

inline const char* parse_value(const char* b, const char* e, Value* r) {
    if (*b == '"') {
        return parse_string(b, e, r);
    } else if (*b == '{') {
        return parse_object(b, e, new (r) Value(Value::JObject()));
    } else if (*b == '[') {
        return parse_array(b, e, new (r) Value(Value::JArray()));
    } else if (*b == 'f') {
        return parse_false(b, e, r);
    } else if (*b == 't') {
        return parse_true(b, e, r);
    } else if (*b == 'n') {
        return parse_null(b, e, r);
    } else {
        return parse_number(b, e, r);
    }
}

static inline bool parse(const char* b, const char* e, Value* r) {
    while (b < e && is_white_char(*b)) ++b;
    if (b == e) return false;

    if (*b == '{') {
        b = parse_object(b, e, new (r) Value(Value::JObject()));
    } else if (*b == '[') {
        b = parse_array(b, e, new (r) Value(Value::JArray()));
    } else {
        b = parse_value(b, e, r);
    }

    if (b == 0) return false;
    while (++b < e && is_white_char(*b));
    return b == e;
}

static const char* parse_object(const char* b, const char* e, Value* r) {
    char* key;
    void* val;
    static int obn = 0;

    while (true) {
        while (++b < e && is_white_char(*b));
        if (b == e) return 0;
        if (*b == '}') return b; // object end

        // key
        b = parse_key(b, e, &key);
        if (b == 0) return 0;

        // ':'
        while (++b < e && is_white_char(*b));
        if (b == e || *b != ':') {
            Value::Jalloc::instance()->dealloc_string(key);
            return 0;
        }

        while (++b < e && is_white_char(*b));
        if (b == e) {
            Value::Jalloc::instance()->dealloc_string(key);
            return 0;
        }

        // value
        val = 0;
        b = parse_value(b, e, (Value*)&val);
        if (b == 0) {
            if (val != 0) ((Value*)&val)->~Value();
            Value::Jalloc::instance()->dealloc_string(key);
            return 0;
        }

        auto o = (Value::Array*)(*(char**)r + 8);
        o->push_back(key);
        o->push_back(val);
        ;

        // check value end
        while (++b < e && is_white_char(*b));
        if (b == e) return 0;
        if (*b == '}') return b; // object end
        if (*b != ',') return 0;
    }
}

static const char* parse_array(const char* b, const char* e, Value* r) {
    static int arn = 0;
    while (true) {
        while (++b < e && is_white_char(*b));
        if (b == e) return 0;
        if (*b == ']') return b; // array end

        void* v = 0;
        b = parse_value(b, e, (Value*)&v);
        if (b == 0) {
            if (v) ((Value*)&v)->~Value();
            return 0;
        }

        auto a = (Value::Array*)(*(char**)r + 8);
        a->push_back(v);

        while (++b < e && is_white_char(*b));
        if (b == e) return 0;
        if (*b == ']') return b; // array end
        if (*b != ',') return 0;
    }
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

static const char* parse_string(const char* b, const char* e, Value* r) {
    const char* p = find_quote_or_escape(++b, e); // find the first '"' or '\\'
    if (p == 0) return 0;
    if (*p == '"') {
        new (r) Value(b, p - b);
        return p;
    }

    fastream& s = Value::Jalloc::instance()->alloc_stream();
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
            new (r) Value(s.data(), s.size());
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
static const char* parse_unicode(const char* b, const char* e, fastream& s) {
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

static const char* parse_number(const char* b, const char* e, Value* r) {
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
        new (r) Value(*b != '-' ? MAX_UINT64 : MIN_INT64);
        return p - 1;
    }

  to_int:
    new (r) Value(str2int(b, p));
    return p - 1;

  to_dbl:
    double d;
    if (p == e && *p != '\0') {
        fastream& fs = Value::Jalloc::instance()->alloc_stream();
        fs.append(b, p - b);
        b = fs.c_str();
    }
    if (str2double(b, d)) {
        new (r) Value(d);
        return p - 1;
    } else {
        return 0;
    }
}

bool Value::parse_from(const char* s, size_t n) {
    this->reset();
    return parse(s, s + n, this);
}

} // namespace json
