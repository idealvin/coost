#include "co/json.h"
#include <algorithm>

namespace json {
namespace xx {

class Alloc {
  public:
    static const uint32 R = Array::R;
    static const uint32 N = 8192;
    Alloc() : _stack(), _ustack(32), _fs(256) {}

    void* alloc() {
        return !_a[0].empty() ? (void*)_a[0].pop_back() : co::alloc(16);
    }

    void free(void* p) {
        _a[0].size() < (8 * (N - R)) ? _a[0].push_back(p) : co::free(p, 16);
    }

    void* alloc(uint32 n) {
        void* p;
        const uint32 x = (n - 1) >> 4;
        switch (x) {
          case 0:
            p = !_a[0].empty() ? (void*)_a[0].pop_back() : co::alloc(16);
            break;
          case 1:
            p = !_a[1].empty() ? (void*)_a[1].pop_back() : co::alloc(32);
            break;
          case 2:
          case 3:
            p = !_a[2].empty() ? (void*)_a[2].pop_back() : co::alloc(64);
            break;
          case 4:
          case 5:
          case 6:
          case 7:
            p = !_a[3].empty() ? (void*)_a[3].pop_back() : co::alloc(128);
            break;
          default:
            p = co::alloc(n);
        }
        return p;
    }

    void free(void* p, uint32 n) {
        const uint32 x = (n - 1) >> 4;
        switch (x) {
          case 0:
            _a[0].size() < (8 * (N - R)) ? _a[0].push_back(p) : co::free(p, 16);
            break;
          case 1:
            _a[1].size() < (4 * (N - R)) ? _a[1].push_back(p) : co::free(p, 32);
            break;
          case 2:
          case 3:
            _a[2].size() < (2 * (N - R)) ? _a[2].push_back(p) : co::free(p, 64);
            break;
          case 4:
          case 5:
          case 6:
          case 7:
            _a[3].size() < (N - R) ? _a[3].push_back(p) : co::free(p, 128);
            break;
          default:
            co::free(p, n);
        }
    }

    Array& stack() { return _stack; }
    Array& ustack() { return _ustack; }
    fastream& stream() { _fs.clear(); return _fs; }
    Json& null() { _null.reset(); return _null; }

  private:
    Array _a[4];
    Array _stack;
    Array _ustack;
    fastream _fs;
    Json _null;
};

inline Alloc& jalloc() {
    static __thread Alloc* a = 0;
    return a ? *a : *(a = co::static_new<Alloc>()); 
}

void* alloc() {
    return jalloc().alloc();
}

char* alloc_string(const void* p, size_t n) {
    char* s = (char*) jalloc().alloc((uint32)n + 1);
    memcpy(s, p, n);
    s[n] = '\0';
    return s;
}

inline void* alloc_array(void** p, uint32 n) {
    auto h = (Array::_H*) co::alloc(sizeof(Array::_H) + sizeof(void*) * n);
    h->cap = n;
    h->size = n;
    memcpy(h->p, p, sizeof(void*) * n);
    return h;
}

} // xx

using _H = Json::_H;
using _A = xx::Alloc;
typedef const char* S;
typedef void* void_ptr_t;
inline S find_quote(S b, S e) { return (S) memchr(b, '"', e - b); }
inline S find_slash(S b, S e) { return (S) memchr(b, '\\', e - b); }

inline char* make_key(_A& a, const void* p, size_t n) {
    char* s = (char*) a.alloc((uint32)n + 1);
    memcpy(s, p, n);
    s[n] = '\0';
    return s;
}

inline char* make_key(_A& a, const char* p) {
    return make_key(a, p, strlen(p));
}

inline _H* make_string(_A& a, const void* p, size_t n) {
    return new(a.alloc()) _H(p, n);
}

inline _H* make_bool(_A& a, bool v) { return new(a.alloc()) _H(v); }
inline _H* make_int(_A& a, int64 v) { return new(a.alloc()) _H(v); }
inline _H* make_double(_A& a, double v) { return new(a.alloc()) _H(v); }
inline _H* make_object(_A& a) { return new(a.alloc()) _H(Json::_obj_t()); }
inline _H* make_array(_A& a)  { return new(a.alloc()) _H(Json::_arr_t()); }

// json parser
//   @b: beginning of the string
//   @e: end of the string
// return the current position, or NULL on any error
class Parser {
  public:
    Parser() : _a(xx::jalloc()) {}
    ~Parser() = default;

    bool parse(S b, S e, void_ptr_t& v);
    S parse_string(S b, S e, void_ptr_t& v);
    S parse_unicode(S b, S e, fastream& s);
    S parse_number(S b, S e, void_ptr_t& v);
    S parse_key(S b, S e, void_ptr_t& k);
    S parse_false(S b, S e, void_ptr_t& v);
    S parse_true(S b, S e, void_ptr_t& v);
    S parse_null(S b, S e, void_ptr_t& v);

  private:
    xx::Alloc& _a;
};

inline S Parser::parse_key(S b, S e, void_ptr_t& key) {
    if (*b++ != '"') return 0;
    S p = (S) memchr(b, '"', e - b);
    if (p) key = make_key(_a, b, p - b);
    return p;
}

inline S Parser::parse_false(S b, S e, void_ptr_t& v) {
    if (e - b >= 5 && b[1] == 'a' && b[2] == 'l' && b[3] == 's' && b[4] == 'e') {
        v = make_bool(_a, false);
        return b + 4;
    }
    return 0;
}

inline S Parser::parse_true(S b, S e, void_ptr_t& v) {
    if (e - b >= 4 && b[1] == 'r' && b[2] == 'u' && b[3] == 'e') {
        v = make_bool(_a, true);
        return b + 3;
    }
    return 0;
}

inline S Parser::parse_null(S b, S e, void_ptr_t& v) {
    if (e - b >= 4 && b[1] == 'u' && b[2] == 'l' && b[3] == 'l') {
        v = 0;
        return b + 3;
    }
    return 0;
}

inline bool is_white_space(char c) {
    return (c == ' ' || c == '\n' || c == '\r' || c == '\t');
}

#if 1
#define skip_white_space(b, e) \
    if (++b < e && is_white_space(*b)) { \
        for (++b;;) { \
            if (b + 8 <= e) { \
                if (!is_white_space(b[0])) break; \
                if (!is_white_space(b[1])) { b += 1; break; } \
                if (!is_white_space(b[2])) { b += 2; break; } \
                if (!is_white_space(b[3])) { b += 3; break; } \
                if (!is_white_space(b[4])) { b += 4; break; } \
                if (!is_white_space(b[5])) { b += 5; break; } \
                if (!is_white_space(b[6])) { b += 6; break; } \
                if (!is_white_space(b[7])) { b += 7; break; } \
                b += 8; \
            } else { \
                if (b + 4 <= e) {\
                    if (!is_white_space(b[0])) break; \
                    if (!is_white_space(b[1])) { b += 1; break; } \
                    if (!is_white_space(b[2])) { b += 2; break; } \
                    if (!is_white_space(b[3])) { b += 3; break; } \
                    b += 4; \
                } \
                if (b == e || !is_white_space(b[0])) break; \
                if (b + 1 == e || !is_white_space(b[1])) { b += 1; break; } \
                if (b + 2 == e || !is_white_space(b[2])) { b += 2; break; } \
                b = e; break; \
            } \
        } \
    }
#else
#define skip_white_space(b, e) \
    while (++b < e && is_white_space(*b));
#endif

// This is a non-recursive implement of json parser.
// stack: |prev size|prev state|val|....
bool Parser::parse(S b, S e, void_ptr_t& val) {
    union { uint32 state; void* pstate; };
    union { uint32 size;  void* psize; };
    void_ptr_t key;
    auto& s = _a.stack();
    auto& u = _a.ustack();
    state = 0, size = 0;

    while (b < e && is_white_space(*b)) ++b;
    if (unlikely(b == e)) return false;
    if (*b == '{') goto obj_beg;
    if (*b == '[') goto arr_beg;
    goto val_beg;

  obj_beg:
    u.push_back(psize);  // prev size
    u.push_back(pstate); // prev state
    s.push_back(make_object(_a));
    size = s.size(); // current size
    state = '{';

  obj_val_beg:
    skip_white_space(b, e);
    if (b == e) goto err;
    if (*b == '}') goto obj_end;

    b = parse_key(b, e, key);
    if (b == 0) goto err;
    s.push_back(key);

    while (++b < e && is_white_space(*b));
    if (b == e || *b != ':') goto err;

    while (++b < e && is_white_space(*b));
    if (b == e) goto err;

    switch (*b) {
      case '"':
        b = parse_string(b, e, val);
        break;
      case '{':
        goto obj_beg;
      case '[':
        goto arr_beg;
      case 'f':
        b = parse_false(b, e, val);
        break;
      case 't':
        b = parse_true(b, e, val);
        break;
      case 'n':
        b = parse_null(b, e, val);
        break;
      default:
        b = parse_number(b, e, val);
    }
    if (b == 0) goto err;
    s.push_back(val);

  obj_val_end:
    skip_white_space(b, e);
    if (b == e) goto err;
    if (*b == ',') goto obj_val_beg;
    if (*b == '}') goto obj_end;
    goto err;

  arr_beg:
    u.push_back(psize);  // prev size
    u.push_back(pstate); // prev state
    s.push_back(make_array(_a));
    size = s.size(); // current size
    state = '[';

  arr_val_beg:
    skip_white_space(b, e);
    if (b == e) goto err;
    if (*b == ']') goto arr_end;

    switch (*b) {
      case '"':
        b = parse_string(b, e, val);
        break;
      case '{':
        goto obj_beg;
      case '[':
        goto arr_beg;
      case 'f':
        b = parse_false(b, e, val);
        break;
      case 't':
        b = parse_true(b, e, val);
        break;
      case 'n':
        b = parse_null(b, e, val);
        break;
      default:
        b = parse_number(b, e, val);
    }
    if (b == 0) goto err;
    s.push_back(val);

  arr_val_end:
    skip_white_space(b, e);
    if (b == e) goto err;
    if (*b == ',') goto arr_val_beg;
    if (*b == ']') goto arr_end;
    goto err;

  arr_end:
  obj_end:
    if (s.size() > size) {
        void* p = xx::alloc_array(s.data() + size, s.size() - size);
        s.resize(size);
        ((_H*)s.back())->p = p;
    }

    pstate = u.pop_back(); // prev state
    if (state == '{') { psize = u.pop_back(); goto obj_val_end; }
    if (state == '[') { psize = u.pop_back(); goto arr_val_end; }
    u.pop_back();
    val = s.pop_back();
    goto end;

  val_beg:
    switch (*b) {
      case '"':
        b = parse_string(b, e, val);
        break;
      case 'f':
        b = parse_false(b, e, val);
        break;
      case 't':
        b = parse_true(b, e, val);
        break;
      case 'n':
        b = parse_null(b, e, val);
        break;
      default:
        b = parse_number(b, e, val);
    }
    if (b == 0) goto err;

  end:
    assert(s.size() == 0);
    assert(u.size() == 0);
    while (++b < e && is_white_space(*b));
    return b == e;

  err:
    while (s.size() > 0) {
        if (s.size() > size) {
            if (state == '{' && ((s.size() - size) & 1)) s.push_back(0);
            void* p = xx::alloc_array(s.data() + size, s.size() - size);
            s.resize(size);
            ((_H*)s.back())->p = p;
        }

        pstate = u.pop_back();
        psize = u.pop_back();
        if (state == 0) {
            val = s.pop_back();
            break;
        }
    }
    assert(s.size() == 0);
    assert(u.size() == 0);
    return false;
}

static inline const char* init_s2e_table() {
    static char tb[256] = { 0 };
    tb[(unsigned char)'r'] = '\r';
    tb[(unsigned char)'n'] = '\n';
    tb[(unsigned char)'t'] = '\t';
    tb[(unsigned char)'b'] = '\b';
    tb[(unsigned char)'f'] = '\f';
    tb[(unsigned char)'"'] = '"';
    tb[(unsigned char)'\\'] = '\\';
    tb[(unsigned char)'/'] = '/';
    tb[(unsigned char)'u'] = 'u';
    return tb;
}

S Parser::parse_string(S b, S e, void_ptr_t& v) {
    S p, q;
    if ((p = find_quote(++b, e)) == 0) return 0;
    q = find_slash(b, p);
    if (q == 0) {
        v = make_string(_a, b, p - b);
        return p;
    }

    fastream& s = _a.stream();
    do {
        s.append(b, q - b);
        if (++q == e) return 0;

        static S tb = init_s2e_table();
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
        
        q = find_slash(b, p);
        if (q == 0) {
            s.append(b, p - b);
            v = make_string(_a, s.data(), s.size());
            return p;
        }
    } while (true);
}

inline const char* init_hex_table() {
    static char tb[256];
    memset(tb, 16, 256);
    for (char c = '0'; c <= '9'; ++c) tb[(uint8)c] = c - '0';
    for (char c = 'A'; c <= 'F'; ++c) tb[(uint8)c] = c - 'A' + 10;
    for (char c = 'a'; c <= 'f'; ++c) tb[(uint8)c] = c - 'a' + 10;
    return tb;
}

inline const char* parse_hex(const char* b, const char* e, uint32& u) {
    static const char* const tb = init_hex_table();
    uint32 u0, u1, u2, u3;
    if (b + 4 <= e) {
        u0 = tb[(uint8)b[0]];
        u1 = tb[(uint8)b[1]];
        u2 = tb[(uint8)b[2]];
        u3 = tb[(uint8)b[3]];
        if (u0 == 16 || u1 == 16 || u2 == 16 || u3 == 16) return 0;
        u = (u0 << 12) | (u1 << 8) | (u2 << 4) | u3;
        return b + 3;
    }
    return 0;
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
S Parser::parse_unicode(S b, S e, fastream& s) {
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

inline int64 str2int(S b, S e) {
    uint64 v = 0;
    S p = b;
    if (*p == '-') ++p;
    for (; p < e; ++p) v = v * 10 + *p - '0';
    return *b != '-' ? v : -(int64)v;
}

inline bool str2double(S b, double& d) {
    errno = 0;
    d = strtod(b, 0);
    return !(errno == ERANGE && (d == HUGE_VAL || d == -HUGE_VAL));
}

inline bool is_digit(char c) {
    return '0' <= c && c <= '9';
}

S Parser::parse_number(S b, S e, void_ptr_t& v) {
    bool is_double = false;
    S p = b;

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
        v = make_int(_a, *b != '-' ? MAX_UINT64 : MIN_INT64);
        return p - 1;
    }

  to_int:
    v = make_int(_a, str2int(b, p));
    return p - 1;

  to_dbl:
    double d;
    if (p == e && *p != '\0') {
        fastream& fs = _a.stream();
        fs.append(b, p - b);
        b = fs.c_str();
    }
    if (str2double(b, d)) {
        v = make_double(_a, d);
        return p - 1;
    }
    return 0;
}

bool Json::parse_from(const char* s, size_t n) {
    if (_h) this->reset();
    Parser parser;
    bool r = parser.parse(s, s + n, *(void**)&_h);
    if (unlikely(!r && _h)) this->reset();
    return r;
}

static inline const char* init_e2s_table() {
    static char tb[256] = { 0 };
    tb[(unsigned char)'\r'] = 'r';
    tb[(unsigned char)'\n'] = 'n';
    tb[(unsigned char)'\t'] = 't';
    tb[(unsigned char)'\b'] = 'b';
    tb[(unsigned char)'\f'] = 'f';
    tb[(unsigned char)'\"'] = '"';
    tb[(unsigned char)'\\'] = '\\';
    return tb;
}

inline const char* find_escapse(const char* b, const char* e, char& c) {
    static const char* tb = init_e2s_table();
  #if 1
    char c0, c1, c2, c3, c4, c5, c6, c7;
    for (;;) {
        if (b + 8 <= e) {
            if ((c0 = tb[(uint8)b[0]])) { c = c0; return b; }
            if ((c1 = tb[(uint8)b[1]])) { c = c1; return b + 1; }
            if ((c2 = tb[(uint8)b[2]])) { c = c2; return b + 2; }
            if ((c3 = tb[(uint8)b[3]])) { c = c3; return b + 3; }
            if ((c4 = tb[(uint8)b[4]])) { c = c4; return b + 4; }
            if ((c5 = tb[(uint8)b[5]])) { c = c5; return b + 5; }
            if ((c6 = tb[(uint8)b[6]])) { c = c6; return b + 6; }
            if ((c7 = tb[(uint8)b[7]])) { c = c7; return b + 7; }
            b += 8;
        } else {
            if (b + 4 <= e) {
                if ((c0 = tb[(uint8)b[0]])) { c = c0; return b; }
                if ((c1 = tb[(uint8)b[1]])) { c = c1; return b + 1; }
                if ((c2 = tb[(uint8)b[2]])) { c = c2; return b + 2; }
                if ((c3 = tb[(uint8)b[3]])) { c = c3; return b + 3; }
                b += 4;
            }
            for (; b < e; ++b) {
                if ((c = tb[(uint8)*b])) return b;
            }
            return e;
        }
    }
  #else
    for (const char* p = b; p < e; ++p) {
        if ((c = tb[(uint8)*p])) return p;
    }
    return e;
  #endif
}

fastream& Json::_json2str(fastream& fs, bool debug, int mdp) const {
    if (!_h) return fs.append("null", 4);

    switch (_h->type) {
      case t_string: {
        fs << '"';
        const uint32 len = _h->size;
        const bool trunc = debug && len > 512;
        S s = _h->s;
        S e = trunc ? s + 32 : s + len;

        char c;
        for (S p; (p = find_escapse(s, e, c)) < e;) {
            fs.append(s, p - s).append('\\').append(c);
            s = p + 1;
        }

        if (s != e) fs.append(s, e - s);
        if (trunc) fs.append(3, '.');
        fs << '"';
        break;
      }

      case t_object: {
        fs << '{';
        if (_h->p) {
            auto& a = *(xx::Array*)&_h->p;
            for (uint32 i = 0; i < a.size(); i += 2) {
                fs << '"' << (S)a[i] << '"' << ':';
                ((Json*)&a[i + 1])->_json2str(fs, debug, mdp) << ',';
            }
        }
        fs.back() == ',' ? (void)(fs.back() = '}') : (void)(fs.append('}'));
        break;
      }

      case t_array: {
        fs << '[';
        if (_h->p) {
            auto& a = *(xx::Array*)&_h->p;
            for (uint32 i = 0; i < a.size(); ++i) {
                ((Json*)&a[i])->_json2str(fs, debug, mdp) << ',';
            }
        }
        fs.back() == ',' ? (void)(fs.back() = ']') : (void)(fs.append(']'));
        break;
      }

      case t_int:
        fs << _h->i;
        break;
      case t_bool:
        fs << _h->b;
        break;
      case t_double:
        fs.maxdp(mdp) << _h->d;
        break;
    }

    return fs;
}

// @indent:  4 spaces by default
// @n:       number of spaces to insert at the beginning for the current line
fastream& Json::_json2pretty(fastream& fs, int indent, int n, int mdp) const {
    if (!_h) return fs.append("null", 4);

    switch (_h->type) {
      case t_object: {
        fs << '{';
        if (_h->p) {
            auto& a = *(xx::Array*)&_h->p;
            for (uint32 i = 0; i < a.size(); i += 2) {
                fs.append('\n').append(n, ' ');
                fs << '"' << (S)a[i] << '"' << ": ";
                ((Json*)&a[i + 1])->_json2pretty(fs, indent, n + indent, mdp) << ',';
            }
        }
        if (fs.back() == ',') {
            fs.back() = '\n';
            if (n > indent) fs.append(n - indent, ' ');
        }
        fs << '}';
        break;
      }

      case t_array: {
        fs << '[';
        if (_h->p) {
            auto& a = *(xx::Array*)&_h->p;
            for (uint32 i = 0; i < a.size(); ++i) {
                fs.append('\n').append(n, ' ');
                ((Json*)&a[i])->_json2pretty(fs, indent, n + indent, mdp) << ',';
            }
        }
        if (fs.back() == ',') {
            fs.back() = '\n';
            if (n > indent) fs.append(n - indent, ' ');
        }
        fs << ']';
        break;
      }

      default:
        _json2str(fs, false, mdp);
    }

    return fs;
}

bool Json::has_member(const char* key) const {
    if (this->is_object()) {
        for (auto it = this->begin(); it != this->end(); ++it) {
            if (strcmp(key, it.key()) == 0) return true;
        }
    }
    return false;
}

Json& Json::operator[](const char* key) const {
    assert(!_h || _h->type & t_object);
    for (auto it = this->begin(); it != this->end(); ++it) {
        if (strcmp(key, it.key()) == 0) return it.value();
    }

    if (!_h) {
        ((Json*)this)->_h = make_object(xx::jalloc());
        new (&_h->p) xx::Array(8);
    }
    if (!_h->p) new (&_h->p) xx::Array(8);

    auto& a = _array();
    a.push_back(make_key(xx::jalloc(), key));
    a.push_back(0);
    return *(Json*)&a.back();
}

Json& Json::get(uint32 i) const {
    if (this->is_array() && _array().size() > i) {
        return *(Json*)&_array()[i];
    }
    return xx::jalloc().null();
}

Json& Json::get(const char* key) const {
    if (this->is_object()) {
        for (auto it = this->begin(); it != this->end(); ++it) {
            if (strcmp(key, it.key()) == 0) return it.value();
        }
    }
    return xx::jalloc().null();
}

void Json::remove(const char* key) {
    if (this->is_object()) {
        const uint32 n = _h->p ? _array().size() : 0;
        if (n > 0) {
            auto& a = _array();
            for (uint32 i = 0; i < n; i += 2) {
                const auto s = (const char*)a[i];
                if (strcmp(key, s) == 0) {
                    xx::jalloc().free((void*)s, (uint32)strlen(s) + 1);
                    ((Json&)a[i + 1]).reset();
                    a.remove_pair(i);
                    return;
                }
            }
        }
    }
}

void Json::erase(const char* key) {
    if (this->is_object()) {
        const uint32 n = _h->p ? _array().size() : 0;
        if (n > 0) {
            auto& a = _array();
            for (uint32 i = 0; i < n; i += 2) {
                const auto s = (const char*)a[i];
                if (strcmp(key, s) == 0) {
                    xx::jalloc().free((void*)s, (uint32)strlen(s) + 1);
                    ((Json&)a[i + 1]).reset();
                    a.erase_pair(i);
                    return;
                }
            }
        }
    }
}

Json& Json::_set(uint32 i) {
  beg:
    if (this->is_null()) {
        for (uint32 k = 0; k < i; ++k) {
            this->push_back(Json());
        }
        this->push_back(Json());
        return *(Json*)&_array()[i];
    }

    if (unlikely(!this->is_array())) {
        this->reset();
        goto beg;
    }

    auto& a = _array();
    if (i < a.size()) {
        return *(Json*)&a[i];
    } else {
        for (uint32 k = a.size(); k < i; ++k) {
            this->push_back(Json());
        }
        this->push_back(Json());
        return *(Json*)&_array()[i]; // don't use `a` here
    }
}

Json& Json::_set(const char* key) {
  beg:
    if (this->is_null()) {
        this->add_member(key, Json());
        return *(Json*)&_array()[1];
    }

    if (unlikely(!this->is_object())) {
        this->reset();
        goto beg;
    }

    for (auto it = this->begin(); it != this->end(); ++it) {
        if (strcmp(key, it.key()) == 0) return it.value();
    }

    this->add_member(key, Json());
    return *(Json*)&_array().back();
}

void Json::reset() {
    if (_h) {
        auto& a = xx::jalloc();
        switch (_h->type) {
          case t_object:
            for (auto it = this->begin(); it != this->end(); ++it) {
                a.free((void*)it.key(), (uint32)strlen(it.key()) + 1);
                it.value().reset();
            }
            if (_h->p) _array().~Array();
            break;

          case t_array:
            for (auto it = this->begin(); it != this->end(); ++it) {
                (*it).reset();
            }
            if (_h->p) _array().~Array();
            break;
          
          case t_string:
            if (_h->s) a.free(_h->s, _h->size + 1);
            break;
        }
        a.free(_h);
        _h = 0;
    }
}

void* Json::_dup() const {
    _H* h = 0;
    if (_h) {
        switch (_h->type) {
          case t_object:
            h = make_object(xx::jalloc());
            if (_h->p) {
                auto& a = *new(&h->p) xx::Array(_array().size());
                for (auto it = this->begin(); it != this->end(); ++it) {
                    a.push_back(make_key(xx::jalloc(), it.key()));
                    a.push_back(it.value()._dup());
                }
            }
            break;
          case t_array:
            h = make_array(xx::jalloc());
            if (_h->p) {
                auto& a = *new(&h->p) xx::Array(_array().size());
                for (auto it = this->begin(); it != this->end(); ++it) {
                    a.push_back((*it)._dup());
                }
            }
            break;
          case t_string:
            h = make_string(xx::jalloc(), _h->s, _h->size);
            break;
          default:
            h = (_H*) xx::jalloc().alloc();
            h->type = _h->type;
            h->i = _h->i;
        }
    }
    return h;
}

Json::Json(std::initializer_list<Json> v) {
    const bool is_obj = std::all_of(v.begin(), v.end(), [](const Json& x) {
        return x.is_array() && x.array_size() == 2 && x[0].is_string();
    });

    if (is_obj) {
        _h = make_object(xx::jalloc());
        const uint32 n = (uint32)(v.size() << 1);
        if (n > 0) {
            auto& a = *new(&_h->p) xx::Array(n);
            for (auto& x : v) {
                a.push_back(x[0]._h->s); x[0]._h->s = 0;
                a.push_back(x[1]._h); x[1]._h = 0;
            }
        }

    } else {
        _h = make_array(xx::jalloc());
        const uint32 n = (uint32)v.size();
        if (n > 0) {
            auto& a = *new(&_h->p) xx::Array(n);
            for (auto& x : v) {
                a.push_back(x._h); ((Json*)&x)->_h = 0;
            }
        }
    }
}

Json array(std::initializer_list<Json> v) {
    Json r = json::array();
    _H* h = *(_H**)&r;
    const uint32 n = (uint32)v.size();
    if (n > 0) {
        auto& a = *new(&h->p) xx::Array(n);
        for (auto& x : v) {
            a.push_back(*(_H**)&x);
            *(_H**)&x = 0;
        }
    }
    return r;
}

Json object(std::initializer_list<Json> v) {
    Json r = json::object();
    _H* h = *(_H**)&r;
    const uint32 n = (uint32)(v.size() << 1);
    if (n > 0) {
        auto& a = *new(&h->p) xx::Array(n);
        for (auto& x : v) {
            assert(x.is_array() && x.size() == 2 && x[0].is_string());
            a.push_back((*(Json::_H**)&x[0])->s);
            (*(_H**)&x[0])->s = 0;
            a.push_back(*(_H**)&x[1]);
            *(_H**)&x[1] = 0;
        }
    }
    return r;
}

} // json
