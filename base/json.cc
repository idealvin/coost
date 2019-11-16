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
                if (a[0] == 0) free((void*) a[1]); // free the keys
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
        const char* p = strpbrk(cs, "\r\n\t\"\\");
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
                } else {
                    fs.append('t');
                }

                cs = p + 1;
                p = strpbrk(cs, "\r\n\t\"\\");

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

        if (*p == '"' || *p == '\\') {
            fs.append(*p);
        } else if (*p == 'n') {
            fs.append('\n');
        } else if (*p == 'r') {
            fs.append('\r');
        } else if (*p == 't') {
            fs.append('\t');
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
                if (dots == 0) {
                    try {
                        new (v) Value(fastatoi(p, b - p));
                    } catch (...) {
                        return 0; // invalid int
                    }
                } else {
                    try {
                        new (v) Value(str::to_double(fastring(p, b - p)));
                    } catch (...) {
                        return 0; // invalid double
                    }
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
static const char* parse_array(const char* b, const char* e, Value* res, fastream& s);

static const char* parse_json(const char* b, const char* e, Value* res, fastream& s) {
    const char* key;

    for (; b < e; ++b) {
        if (unlikely(is_white_char(*b))) continue;
        if (*b++ != '{') return 0; // not object
        res->set_object();
        break;
    }

    for (; b < e; ++b) {
        if (unlikely(is_white_char(*b))) continue;
        if (*b == '}') return b;   // OBJ_END
        if (*b != '"') return 0;   // not key

        // read key
        key = s.data() + s.size(); // pointer to the key
        b = read_key(b + 1, e, s);
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
                b = parse_json(b, e, (Value*)&v, s);
            } else if (*b == '[') {
                b = parse_array(b + 1, e, (Value*)&v, s);
            } else {
                b = read_token(b, e, &v);
            }

            if (b == 0) return 0;
            res->__add_member(key, v);
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

static const char* parse_array(const char* b, const char* e, Value* res, fastream& s) {
    for (; b < e; ++b) {
        if (unlikely(is_white_char(*b))) continue;
        if (*b == ']') return b; // ARR_END

        void* v = 0;
        if (*b == '"') {
            b = read_string(b + 1, e, &v);
        } else if (*b == '{') {
            b = parse_json(b, e, (Value*)&v, s);
        } else if (*b == '[') {
            b = parse_array(b + 1, e, (Value*)&v, s);
        } else {
            b = read_token(b, e, &v);
        }

        if (b == 0) return 0;
        res->__push_back(v);

        for (++b; b < e; ++b) {
            if (unlikely(is_white_char(*b))) continue;
            if (*b == ']') return b; // ARR_END
            if (*b != ',') return 0;
            break;
        }
    }

    return 0;
}

bool Value::parse_from(const char* s, size_t n) {
    if (unlikely(n < 2)) return false;

    this->reset();
    this->__add_member((const char*)0, (void*)0); // the first element for save keys

    char buf[sizeof(fastream)];
    fastream* keys = new (buf) fastream((n >> 1) + 1);

    const char* p = parse_json(s, s + n, this,  *keys);
    if (unlikely(p == 0)) {
        keys->~fastream();
        return false;
    }

    for (const char* x = p + 1; x < s + n; ++x) {
        if (is_white_char(*x)) continue;
        keys->~fastream();
        return false;
    }

    _Array()[1] = keys->data();
    return true;
}

} // namespace json
