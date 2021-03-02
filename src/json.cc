#include "co/json.h"
#include "co/str.h"

namespace json {

Value::Jalloc::~Jalloc() {
    for (uint32 i = 0; i < _mp.size(); ++i) free((void*)_mp[i]);
    for (uint32 i = 0; i < _ks[0].size(); ++i) free((char*)_ks[0][i] - 8);
    for (uint32 i = 0; i < _ks[1].size(); ++i) free((char*)_ks[1][i] - 8);
    for (uint32 i = 0; i < _ks[2].size(); ++i) free((char*)_ks[2][i] - 8);
}

void* Value::Jalloc::alloc(uint32 n) {
    char* p;
    if (n <= 24) {
        if (!_ks[0].empty()) return (void*) _ks[0].pop_back();
        p = (char*) malloc(32);
        *p = 0;
    } else if (n <= 56) {
        if (!_ks[1].empty()) return (void*) _ks[1].pop_back();
        p = (char*) malloc(64);
        *p = 1;
    } else if (n <= 120) {
        if (!_ks[2].empty()) return (void*) _ks[2].pop_back();
        p = (char*) malloc(128);
        *p = 2;
    } else {
        p = (char*) malloc(n + 8);
        *p = 3;
    }

    return p + 8;
}

void Value::Jalloc::dealloc(void* p) {
    char* s = (char*)p - 8;
    int c = *s; // 0, 1, 2, 3
    if (c < 3 && _ks[c].size() < 4095) {
        _ks[c].push_back(p);
    } else {
        free(s);
    }
}

Value& Value::operator[](Key key) const {
    if (_mem) assert(_mem->type & kObject);
    else new ((void*)&_mem) Value(Value::JObject());

    for (auto it = this->begin(); it != this->end(); ++it) {
        if (strcmp(it->key, key) == 0) return it->value;
    }

    _mem->a.push_back(_Alloc_key(key));
    _mem->a.push_back(0); // null value 
    return *(Value*) &_mem->a.back();
}

Value Value::find(Key key) const {
    if (this->is_object()) {
        for (auto it = this->begin(); it != this->end(); ++it) {
            if (strcmp(it->key, key) == 0) return it->value;
        }
    }
    return Value();
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
    if (atomic_dec(&_mem->refn) == 0) {
        if (_mem->type & kObject) {
            Array& a = _mem->a;
            for (uint32 i = 0; i < a.size(); i += 2) {
                Jalloc::instance()->dealloc((void*)a[i]);
                ((Value*)&a[i + 1])->~Value();
            }
            a.~Array();
        } else if (_mem->type & kArray) {
            Array& a = _mem->a;
            for (uint32 i = 0; i < a.size(); ++i) {
                ((Value*)&a[i])->~Value();
            }
            a.~Array();
        } else if (_mem->type & kString) {
            Jalloc::instance()->dealloc(_mem->s);
        }
        Jalloc::instance()->dealloc_mem(_mem);
    }
}

static inline const char* init_table() {
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

void Value::_Json2str(fastream& fs, bool debug) const {
    if (_mem == 0) {
        fs << "null";

    } else if (_mem->type & kString) {
        fs << '"';
        static const char* tb = init_table();
        uint32 len = _mem->l[-1];
        bool trunc = debug && len > 256;
        const char* s = _mem->s;
        const char* e = trunc ? s + 256 : s + len;

        for (const char* p = s; p != e; ++p) {
            char c = tb[(uint8)*p];
            if (c) {
                fs.append(s, p - s).append('\\').append(c);
                s = p + 1;
            }
        }

        if (s != e) fs.append(s, e - s);
        if (trunc) fs.append(3, '.');
        fs << '"';

    } else if (_mem->type & kObject) {
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

    } else if (_mem->type & kArray) {
        fs << '[';
        auto& a = _mem->a;
        if (!a.empty()) {
            ((Value*)&a[0])->_Json2str(fs, debug);
            for (uint32 i = 1; i < a.size(); ++i) {
                fs << ',';
                ((Value*)&a[i])->_Json2str(fs, debug);
            }
        }
        fs << ']';

    } else {
        if (_mem->type & kInt) {
            fs << _mem->i;
        } else if (_mem->type & kBool) {
            fs << _mem->b;
        } else {
            assert(_mem->type & kDouble);
            fs << _mem->d;
        }
    }
}

// @indent:  4 spaces by default
// @n:       number of spaces to insert at the beginning for the current line
void Value::_Json2pretty(fastream& fs, int indent, int n) const {
    if (_mem == 0) {
        fs << "null";

    } else if (_mem->type & kObject) {
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

    } else if (_mem->type & kArray) {
        fs << '[';
        auto& a = _mem->a;
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
inline bool is_white_char(char c) {
    return (c == ' ' || c == '\n' || c == '\r' || c == '\t');
}

const char* parse_object(const char* b, const char* e, Value* r);
const char* parse_array(const char* b, const char* e, Value* r);

// @b: beginning of the string
// @e: end of the string
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
    while (b < e && is_white_char(*b)) ++b;
    return b == e;
}

const char* parse_key(const char* b, const char* e, char** key) {
    if (*b++ != '"') return 0;
    const char* p = (const char*) memchr(b, '"', e - b);
    if (p ) {
        char* s = (char*) Json::Jalloc::instance()->alloc((uint32)(p - b + 1));
        memcpy(s, b, p - b);
        s[p - b] = '\0';
        *key = s;
    }
    return p;
}

const char* parse_string(const char* b, const char* e, Value* r);
const char* parse_number(const char* b, const char* e, Value* r);

inline const char* parse_false(const char* b, const char* e, Value* r) {
    if (b[1] != 'a' || b[2] != 'l' || b[3] != 's' || b[4] != 'e') return 0;
    new (r) Value(false);
    return b + 5;
}

inline const char* parse_true(const char* b, const char* e, Value* r) {
    if (b[1] != 'r' || b[2] != 'u' || b[3] != 'e') return 0;
    new (r) Value(true);
    return b + 4;
}

inline const char* parse_null(const char* b, const char* e, Value* r) {
    if (b[1] != 'u' || b[2] != 'l' || b[3] != 'l') return 0;
    return b + 4;
}

const char* parse_value(const char* b, const char* e, Value* r) {
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

const char* parse_object(const char* b, const char* e, Value* res) {
    char* key = 0;
    void* val;

    for (; ++b < e;) {
        if (is_white_char(*b)) continue;
        if (*b == '}') return b;

        // key
        b = parse_key(b, e, &key);
        if (b == 0) return 0;

        // ':'
        while (++b < e && is_white_char(*b));
        if (b == e || *b != ':') goto free_key;

        while (++b < e && is_white_char(*b));
        if (b == e) goto free_key;

        // value
        val = 0;
        b = parse_value(b, e, (Value*)&val);
        if (b == 0) goto free_val;

        res->add_member(key, std::move(*(Value*)&val));

        // check value end
        while (++b < e && is_white_char(*b));
        if (b == e) return 0;
        if (*b == '}') return b;
        if (*b != ',') return 0;
    }

    return 0;

  free_val:
    if (val) ((Value*)&val)->~Value();
  free_key:
    Value::Jalloc::instance()->dealloc(key);
  end:
    return 0;
}

const char* parse_array(const char* b, const char* e, Value* res) {
    for (; b < e; ++b) {
        if (unlikely(is_white_char(*b))) continue;
        if (*b == ']') return b; // ARR_END

        void* v = 0;
        if (*b == '"') {
            b = read_string(b + 1, e, &v);
        } else if (*b == '{') {
            b = parse_object(b + 1, e, new (&v) Value(Value::JObject()));
        } else if (*b == '[') {
            b = parse_array(b + 1, e, new (&v) Value(Value::JArray()));
        } else {
            b = read_token(b, e, &v);
        }

        if (unlikely(b == 0)) {
            if (v) ((Value*)&v)->~Value();
            return 0;
        }

        res->_mem->a.push_back(v);

        for (++b; b < e; ++b) {
            if (unlikely(is_white_char(*b))) continue;
            if (*b == ']') return b; // ARR_END
            if (*b != ',') return 0;
            break;
        }
    }

    return 0;
}


// Read unicode sequences as a UTF8 string.
// The following function is neally taken from jsoncpp, all rights belongs to JSONCPP.
// See more details on https://github.com/open-source-parsers/jsoncpp.
const char* read_unicode(const char* b, const char* e, fastream& fs) {
    if (unlikely(e < b + 3)) return 0;

    unsigned int v = 0;
    for (int i = 0; i < 4; ++i) {
        v <<= 4;
        char c = *b++;
        if ('0' <= c && c <= '9') {
            v += c - '0';
        } else if ('a' <= c && c <= 'f') {
            v += c - 'a' + 10;
        } else if ('A' <= c && c <= 'F') {
            v += c - 'A' + 10;
        } else {
            return 0;
        }
    }

    if (0xd800 <= v && v <= 0xdbff) {
        if (unlikely(e < b + 5)) return 0;
        if (unlikely(*b++ != '\\' || *b++ != 'u')) return 0;

        unsigned int u = 0;
        for (int i = 0; i < 4; ++i) {
            u <<= 4;
            char c = *b++;
            if ('0' <= c && c <= '9') {
                u += c - '0';
            } else if ('a' <= c && c <= 'f') {
                u += c - 'a' + 10;
            } else if ('A' <= c && c <= 'F') {
                u += c - 'A' + 10;
            } else {
                return 0;
            }
        }

        if (unlikely(u < 0xdc00 || u > 0xdfff)) return 0;
        v = 0x10000 + ((v & 0x3ff) << 10) + (u & 0x3ff);
    }

    // convert to UTF8
    if (v <= 0x7f) {
        fs.append((char) v);
    } else if (v <= 0x7ff) {
        fs.append((char) (0xc0 | (0x1f & (v >> 6))));
        fs.append((char) (0x80 | (0x3f & v)));
    } else if (v <= 0xffff) {
        fs.append((char) (0xe0 | (0x0f & (v >> 12))));
        fs.append((char) (0x80 | (0x3f & (v >> 6))));
        fs.append((char) (0x80 | (0x3f & v)));
    } else if (v <= 0x10ffff) {
        fs.append((char) (0xf0 | (0x07 & (v >> 18))));
        fs.append((char) (0x80 | (0x3f & (v >> 12))));
        fs.append((char) (0x80 | (0x3f & (v >> 6))));
        fs.append((char) (0x80 | (0x3f & v)));
    } else {
        return 0;
    }

    return b - 1;
}

inline const char* read_string(const char* b, const char* e, void** v) {
    const char* p = strpbrk(b, "\"\\"); // find the first '\\' or '"'
    if (unlikely(!p || p >= e)) return 0;
    if (*p == '"') {
        new (v) Value(b, p - b);
        return p;
    }

    fastream fs;
    do {
        fs.append(b, p - b);
        if (unlikely(++p == e)) return 0;

        if (*p == '"' || *p == '\\' || *p == '/') {
            fs.append(*p);
        } else if (*p == 'u') {
            p = read_unicode(p + 1, e, fs);
            if (!p) return 0;
        } else if (*p == 'n') {
            fs.append('\n');
        } else if (*p == 'r') {
            fs.append('\r');
        } else if (*p == 't') {
            fs.append('\t');
        } else if (*p == 'b') {
            fs.append('\b');
        } else if (*p == 'f') {
            fs.append('\f');
        } else {
            return 0;
        }

        b = p + 1;
        p = strpbrk(b, "\"\\");
        if (unlikely(!p || p >= e)) return 0;

        if (*p == '"') {
            fs.append(b, p - b);
            new (v) Value(fs.data(), fs.size());
            return p;
        }

    } while (true);
}

int64 fastatoi(const char* s, size_t n) {
    uint64 v = 0;
    size_t i = 0;

    if (*s != '-') { /* positiv */
        if (unlikely(*s == '+')) { ++s; --n; }
        if (unlikely(n == 0)) throw 0;

        for (; i < n - 1; ++i) {
            if (unlikely(s[i] < '0' || s[i] > '9')) throw 0;
            v = v * 10 + s[i] - '0';
        }

        if (unlikely(s[i] < '0' || s[i] > '9')) throw 0;

        if (n < 20) return v * 10 + s[i] - '0';

        if (n == 20) {
            if (v > (MAX_UINT64 - (s[i] - '0')) / 10) throw 0;
            return v * 10 + s[i] - '0';
        }

        throw 0; // n > 20

    } else {
        ++s;
        --n;
        if (unlikely(n == 0)) throw 0;

        for (; i < n - 1; ++i) {
            if (unlikely(s[i] < '0' || s[i] > '9')) throw 0;
            v = v * 10 + s[i] - '0';
        }

        if (unlikely(s[i] < '0' || s[i] > '9')) throw 0;

        if (n < 19) return -static_cast<int64>(v * 10 + s[i] - '0');

        if (n == 19) {
            if (v > (static_cast<uint64>(MIN_INT64) - (s[i] - '0')) / 10) throw 0;
            return -static_cast<int64>(v * 10 + s[i] - '0');
        }

        throw 0; // n > 19
    }
}

inline const char* read_token(const char* b, const char* e, void** v) {
    bool is_double = false;
    const char* p = b++;

    for (; b < e; ++b) {
        char c = *b;

        if (c == ',' || c == '}' || c == ']' || is_white_char(c)) {
            if (*p == 'f') {
                if (b - p != 5 || memcmp(p, "false", 5) != 0) return 0;
                new (v) Value(false);
            } else if (*p == 't') {
                if (b - p != 4 || memcmp(p, "true", 4) != 0) return 0;
                new (v) Value(true);
            } else if (unlikely(*p == 'n')) {
                if (b - p != 4 || memcmp(p, "null", 4) != 0) return 0;
                new (v) Value();
            } else {
                try {
                    if (!is_double) {
                        new (v) Value(fastatoi(p, b - p));
                    } else {
                        new (v) Value(str::to_double(fastring(p, b - p)));
                    }
                } catch (...) {
                    return 0; // invalid number
                }
            }
            return b - 1;

        } else if (c == '.' || c == 'e' || c == 'E') {
            is_double = true;
        }
    }

    return 0;
}


bool Value::parse_from(const char* s, size_t n) {
    if (unlikely(_mem)) {
        this->_UnRef();
        _mem = 0;
    }

    const char* p = s;
    const char* e = s + n;
    while (p < e && is_white_char(*p)) ++p;
    if (unlikely(p == e)) return false;

    if (*p == '{') {
        p = parse_object(p + 1, e, new (&_mem) Value(JObject()));
    } else if (*p == '[') {
        p = parse_array(p + 1, e, new (&_mem) Value(JArray()));
    } else {
        return false;
    }

    if (!p) return false;

    for (; ++p < e;) {
        if (!is_white_char(*p)) return false;
    }

    return true;
}

} // namespace json
