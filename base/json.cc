#include "json.h"
#include "str.h"

namespace json {

Value Value::find(Key key) const {
    if (this->is_object()) {
        for (auto it = this->begin(); it != this->end(); ++it) {
            if (strcmp(it->key, key) == 0) return it->value;
        }
    }
    return Value();
}

Value& Value::operator[](Key key) const {
    ((Value*)this)->_Init_object();
    for (auto it = this->begin(); it != this->end(); ++it) {
        if (strcmp(it->key, key) == 0) return it->value;
    }

    _Array().push_back(key);
    _Array().push_back(0); // empty Value
    return *(Value*) &_Array().back();
}

void Value::reset() {
    if (_mem == 0) return;
    if (--_mem->refn == 0) {
        if (_mem->type == kObject) {
            Array& a = _Array();
            if (!a.empty()) {
                if (a[0] == 0 && --(*(uint32*)a[1]) == 0) {
                    free((void*) a[1]); // free the keys
                }
                for (uint32 i = (a[0] ? 1 : 3); i < a.size(); i += 2) {
                    ((Value*) &a[i])->~Value();
                }
            }
        } else if (_mem->type == kArray) {
            for (uint32 i = 0; i < _Array().size(); ++i) {
                ((Value*) &_Array()[i])->~Value();
            }
        }
        free(_mem);
    }
    _mem = 0;
}

void Value::_Json2str(fastream& fs) const {
    if (unlikely(_mem == 0)) {
        fs << "null";
        return;
    }

    if (_mem->type == kString) {
        fs << '"';
        const char* cs = (const char*)(_mem + 1);
        const char* p = strpbrk(cs, "\r\n\t\b\f\"\\");
        if (!p) {
            fs.append(cs, _mem->slen);
        } else {
            do {
                fs.append(cs, p - cs).append('\\');
                if (*p == '\\' || *p == '"') {
                    fs.append(*p);
                } else if (*p == '\n') {
                    fs.append('n');
                } else if (*p == '\r') {
                    fs.append('r');
                } else if (*p == '\t') {
                    fs.append('t');
                } else if (*p == '\b') {
                    fs.append('b');
                } else {
                    fs.append('f');
                }

                cs = p + 1;
                p = strpbrk(cs, "\r\n\t\b\f\"\\");

                if (!p) {
                    fs.append(cs);
                    break;
                }
            } while (true);
        }
        fs << '"';
        return;
    }

    if (_mem->type == kObject) {
        auto it = this->begin();
        if (unlikely(it == this->end())) {
            fs << "{}";
            return;
        }

        fs << '{';
        fs << '"' << it->key << '"' << ':';
        it->value._Json2str(fs);

        for (; ++it != this->end();) {
            fs << ',' << '"' << it->key << '"' << ':';
            it->value._Json2str(fs);
        }

        fs << '}';
        return;
    }

    if (_mem->type == kArray) {
        fs << '[';
        auto& a = _Array();
        if (!a.empty()) ((Value*) &a[0])->_Json2str(fs);
        for (uint32 i = 1; i < a.size(); ++i) {
            fs << ',';
            ((Value*) &a[i])->_Json2str(fs);
        }
        fs << ']';
        return;
    }

    switch (_mem->type) {
      case kInt:
        fs << _mem->i;
        break;
      case kBool:
        fs << _mem->b;
        break;
      case kDouble:
        fs << _mem->d;
        break;
    }
}

void Value::_Json2pretty(int base_indent, int current_indent, fastream& fs) const {
    if (unlikely(_mem == 0)) {
        fs << "null";
        return;
    }

    if (_mem->type == kObject) {
        auto it = this->begin();
        if (unlikely(it == this->end())) {
            fs << "{}";
            return;
        }

        fs << '{' << '\n';

        for (;;) {
            fs.append(' ', current_indent);
            fs << '"' << it->key << '"' << ": ";

            Value& v = it->value;

            if (v.is_object() || v.is_array()) {
                v._Json2pretty(base_indent, current_indent + base_indent, fs);
            } else {
                v._Json2str(fs);
            }

            if (++it != this->end()) {
                fs << ',' << '\n';
            } else {
                fs << '\n';
                break;
            }
        }

        current_indent -= base_indent;
        if (current_indent > 0) fs.append(' ', current_indent);
        fs << '}';
        return;

    } else if (_mem->type == kArray) {
        auto& a = _Array();
        if (unlikely(a.empty())) {
            fs << "[]";
            return;
        }

        fs << '[' << '\n';

        for (uint32 i = 0;;) {
            fs.append(' ', current_indent);

            Value& v = *(Value*) &a[i];
            if (v.is_object() || v.is_array()) {
                v._Json2pretty(base_indent, current_indent + base_indent, fs);
            } else {
                v._Json2str(fs);
            }

            if (++i < a.size()) {
                fs << ',' << '\n';
            } else {
                fs << '\n';
                break;
            }
        }

        current_indent -= base_indent;
        if (current_indent > 0) fs.append(' ', current_indent);
        fs << ']';
        return;
    }

    _Json2str(fs);
}

// json parser
inline bool is_white_char(char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
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

inline const char* read_key(const char* b, const char* e, fastream& s) {
    const char* p = (const char*) memchr(b, '"', e - b);
    if (p ) {
        s.append(b, p - b);
        s.append('\0');
    }
    return p;
}

inline int64 fastatoi(const char* s, size_t n) {
    int64 v = 0;
    bool neg = (*s == '-');

    for (size_t i = neg; i < n; ++i) {
        if (unlikely(s[i] < '0' || s[i] > '9')) throw 0;
        v = v * 10 + s[i] - '0';
    }

    return neg ? -v : v;
}

inline const char* read_token(const char* b, const char* e, void** v) {
    int dots = 0;
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
                    if (dots == 0) {
                        new (v) Value(fastatoi(p, b - p));
                    } else {
                        new (v) Value(str::to_double(fastring(p, b - p)));
                    }
                } catch (...) {
                    return 0; // invalid number
                }
            }
            return b - 1;

        } else if (c == '.') {
            if (++dots > 1) return 0;
        }
    }

    return 0;
}

/*
 * b: beginning of the string.
 * e: end of the string.
 * res: parse result
 * s: save the keys
 */
const char* parse_array(const char* b, const char* e, Value* res, fastream* s, size_t keys_len);

const char* parse_json(const char* b, const char* e, Value* res, fastream* s, size_t keys_len) {
    const char* key;
    res->set_object();
    if (keys_len > 0) {
        if (s->capacity() == 0) {
            new (s) fastream(keys_len);
            s->append((uint32)1);
        } else {
            ++(*(uint32*)s->data());
        }
        res->_Array().push_back(0);
        res->_Array().push_back(s->data());
    }

    for (; b < e; ++b) {
        if (unlikely(is_white_char(*b))) continue;
        if (*b == '}') return b;   // OBJ_END
        if (*b != '"') return 0;   // not key

        // read key
        key = s->data() + s->size(); // pointer to the key
        b = read_key(b + 1, e, *s);
        if (b == 0) return 0;

        // read ':'
        for (; ++b < e;) {
            if (unlikely(is_white_char(*b))) continue;
            if (*b != ':') return 0;
            break;
        }

        // read value
        for (; ++b < e;) {
            if (unlikely(is_white_char(*b))) continue;

            void* v = 0;
            if (*b == '"') {
                b = read_string(b + 1, e, &v);
            } else if (*b == '{') {
                b = parse_json(b + 1, e, (Value*)&v, s, keys_len);
            } else if (*b == '[') {
                b = parse_array(b + 1, e, (Value*)&v, s, keys_len);
            } else {
                b = read_token(b, e, &v);
            }

            if (unlikely(b == 0)) {
                if (v) ((Value*)&v)->~Value();
                return 0;
            }

            res->_Array().push_back(key);
            res->_Array().push_back(v);
            break;
        }

        // check value end
        for (; ++b < e;) {
            if (unlikely(is_white_char(*b))) continue;
            if (*b == '}') return b; // OBJ_END
            if (*b != ',') return 0;
            break;
        }
    }

    return 0;
}

const char* parse_array(const char* b, const char* e, Value* res, fastream* s, size_t keys_len) {
    res->set_array();

    for (; b < e; ++b) {
        if (unlikely(is_white_char(*b))) continue;
        if (*b == ']') return b; // ARR_END

        void* v = 0;
        if (*b == '"') {
            b = read_string(b + 1, e, &v);
        } else if (*b == '{') {
            b = parse_json(b + 1, e, (Value*)&v, s, keys_len);
        } else if (*b == '[') {
            b = parse_array(b + 1, e, (Value*)&v, s, keys_len);
        } else {
            b = read_token(b, e, &v);
        }

        if (unlikely(b == 0)) {
            if (v) ((Value*)&v)->~Value();
            return 0;
        }

        res->_Array().push_back(v);

        for (++b; b < e; ++b) {
            if (unlikely(is_white_char(*b))) continue;
            if (*b == ']') return b; // ARR_END
            if (*b != ',') return 0;
            break;
        }
    }

    return 0;
}

bool Value::parse_from(const char* s, size_t n, size_t buflen) {
    if (unlikely(_mem)) this->reset();

    const char* p = s;
    const char* e = s + n;
    while (p < e && is_white_char(*p)) ++p;
    if (unlikely(p == e)) return false;

    char buf[sizeof(fastream)] = { 0 };
    if (buflen == 0 && n > 4) buflen = n;

    if (*p == '{') {
        p = parse_json(p + 1, e, this, (fastream*)buf, buflen);
    } else if (*p == '[') {
        p = parse_array(p + 1, e, this, (fastream*)buf, buflen);
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
