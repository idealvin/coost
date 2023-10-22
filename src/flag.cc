#include "co/flag.h"
#include "co/cout.h"
#include "co/fs.h"
#include "co/os.h"
#include "co/str.h"
#include "co/stl.h"

DEF_string(help, "", ">>.help info");
DEF_string(config, "", ">>.path of config file", conf);
DEF_string(version, "", ">>.version of the program");
DEF_bool(mkconf, false, ">>.generate config file");
DEF_bool(daemon, false, ">>#0 run program as a daemon");

namespace flag {
namespace xx {

struct Flag;

struct Mod {
    Mod() = default;
    ~Mod() = default;

    void add_flag(
        char iden, const char* name, const char* value, const char* help,
        const char* file, int line, void* addr, const char* alias
    );

    Flag* find_flag(const char* name);

    fastring set_flag_value(const char* name, const fastring& value);
    fastring set_bool_flags(const char* name);
    fastring alias(const char* name, const char* new_name);

    void print_flags();
    void print_all_flags();
    void print_help();

    void make_config(const fastring& exe);
    void parse_config(const fastring& config);
    co::vector<fastring> parse_commandline(int argc, char** argv);
    co::vector<fastring> analyze_args(
        const co::vector<fastring>& args, co::map<fastring, fastring>& kv,
        co::vector<fastring>& bools
    );

    co::map<const char*, Flag*> flags;
};

static Mod* g_mod;

inline Mod& mod() {
    return g_mod ? *g_mod : *(g_mod = co::_make_static<Mod>());
}

struct Flag {
    Flag(char iden, const char* name, const char* alias, const char* value,
         const char* help, const char* file, int line, void* addr);

    fastring set_value(const fastring& v);
    fastring get_value() const;
    void print() const;
    const char* type() const;

    char iden;
    char lv;            // level: 0-9
    bool inco;          // flag inside co (comment starts with >>)
    const char* name;
    const char* alias;  // alias for this flag
    const char* value;  // default value
    const char* help;   // help info
    const char* file;   // file where the flag is defined
    int line;           // line of the file where the flag is defined
    void* addr;         // point to the flag variable
};

Flag::Flag(
    char iden, const char* name, const char* alias, const char* value, 
    const char* help, const char* file, int line, void* addr
) : iden(iden), lv('5'), inco(false), name(name), alias(alias), value(value),
    help(help), file(file), line(line), addr(addr) {
    if (help[0] == '>' && help[1] == '>') { /* flag defined in co */
        this->inco = true;
        this->help += 2;
    }

    // get level(0-9) at the beginning of help
    const char* const h = this->help;
    if (h[0] == '#' && '0' <= h[1] && h[1] <= '9' && (h[2] == ' ' || h[2] == '\0')) {
        lv = h[1];
        this->help += 2 + !!h[2];
    }
}

fastring Flag::set_value(const fastring& v) {
    switch (this->iden) {
      case 's':
        *static_cast<fastring*>(this->addr) = v;
        return fastring();
      case 'b':
        *static_cast<bool*>(this->addr) = str::to_bool(v);
        break;
      case 'i':
        *static_cast<int32*>(this->addr) = str::to_int32(v);
        break;
      case 'u':
        *static_cast<uint32*>(this->addr) = str::to_uint32(v);
        break;
      case 'I':
        *static_cast<int64*>(this->addr) = str::to_int64(v);
        break;
      case 'U':
        *static_cast<uint64*>(this->addr) = str::to_uint64(v);
        break;
      case 'd':
        *static_cast<double*>(this->addr) = str::to_double(v);
        break;
      default:
        return "unknown flag type";
    }

    switch (co::error()) {
      case 0:
        return fastring();
      case ERANGE:
        return "out of range";
      default:
        return "invalid value";
    }
}

template<typename T>
fastring int2str(T t) {
    if ((0 <= t && t <= 8192) || (t < 0 && t >= -8192)) return str::from(t);

    int i = -1;
    while (t != 0 && (t & 1023) == 0) {
        t >>= 10;
        if (++i >= 4) break;
    }

    fastring s = str::from(t);
    if (i >= 0) s.append("kmgtp"[i]);
    return s;
}

fastring Flag::get_value() const {
    switch (this->iden) {
      case 's':
        return *static_cast<fastring*>(this->addr);
      case 'b':
        return str::from(*static_cast<bool*>(this->addr));
      case 'i':
        return int2str(*static_cast<int32*>(this->addr));
      case 'u':
        return int2str(*static_cast<uint32*>(this->addr));
      case 'I':
        return int2str(*static_cast<int64*>(this->addr));
      case 'U':
        return int2str(*static_cast<uint64*>(this->addr));
      case 'd':
        return str::from(*static_cast<double*>(this->addr));
      default:
        return "unknown flag type";
    }
}

inline const char* Flag::type() const {
    switch (this->iden) {
      case 's': return "string";
      case 'b': return "bool";
      case 'i': return "int32";
      case 'u': return "uint32";
      case 'I': return "int64";
      case 'U': return "uint64";
      case 'd': return "double";
      default:  return "unknown";
    }
}

inline void Flag::print() const {
    cout << color::green << "    -" << this->name;
    if (*this->alias) cout << ", " << this->alias;
    cout.flush();
    cout << color::blue << "  " << this->help << '\n' << color::deflt
         << "\ttype: " << this->type()
         << "\t  default: " << this->value
         << "\n\tfrom: " << this->file
         << endl;
}

void Mod::add_flag(
    char iden, const char* name, const char* value, const char* help, 
    const char* file, int line, void* addr, const char* alias) {
    auto f = co::_make_static<Flag>(iden, name, alias, value, help, file, line, addr);
    auto r = flags.emplace(name, f);
    if (!r.second) {
        cout << "multiple definitions of flag: " << name << ", from "
             << r.first->second->file << " and " << file << endl;
        ::exit(0);
    }

    if (alias[0]) {
        auto v = str::split(alias, ',');
        for (auto& x : v) {
            x.trim();
            const size_t n = x.size() + 1;
            char* s = (char*) co::_salloc(n);
            memcpy(s, x.c_str(), n);
            auto r = flags.emplace(s, f);
            if (!r.second) {
                cout << "alias " << name << " as " << x << " failed, flag " << x
                     << " already exists in " << r.first->second->file << endl;
                ::exit(0);
            }
        }
    }
}

inline Flag* Mod::find_flag(const char* name) {
    auto it = flags.find(name);
    return it != flags.end() ? it->second : NULL;
}

fastring Mod::alias(const char* name, const char* new_name) {
    fastring e;
    auto f = this->find_flag(name);
    if (!f) {
        e << "flag not found: " << name;
        return e;
    }

    if (!*new_name) {
        e << "new name is empty";
        return e;
    }

    auto r = flags.emplace(new_name, f);
    if (!r.second) {
        e << "name already exists: " << new_name;
        return e;
    }

    f->alias = new_name;
    return e;
}

fastring Mod::set_flag_value(const char* name, const fastring& value) {
    fastring e;
    Flag* f = this->find_flag(name);
    if (f) {
        e = f->set_value(value);
        if (!e.empty()) e << ": " << value;
    } else {
        e << "flag not defined: " << name;
    }
    return e;
}

// set_bool_flags("abc"):  -abc -> true  or  -a, -b, -c -> true
fastring Mod::set_bool_flags(const char* name) {
    fastring e;
    Flag* f = this->find_flag(name);
    if (f) {
        if (f->iden == 'b') {
            *static_cast<bool*>(f->addr) = true;
        } else {
            e << "value not set for non-bool flag: " << name;
        }
        return e;
    }

    const size_t n = strlen(name);
    if (n == 1) {
        e << "undefined bool flag: " << name;
        return e;
    }

    char sub[2] = { 0 };
    for (size_t i = 0; i < n; ++i) {
        sub[0] = name[i];
        f = this->find_flag(sub);
        if (f) {
            if (f->iden == 'b') {
                *static_cast<bool*>(f->addr) = true;
                continue;
            }
            e << '-' << sub[0] << " is not bool in -" << name;
            return e;
        }
        e << "undefined bool flag -" << sub[0] << " in -" << name;
        return e;
    }

    return e;
}

// print flags not in co
void Mod::print_flags() {
    bool the_first_one = true;
    for (auto it = flags.begin(); it != flags.end(); ++it) {
        const Flag& f = *it->second;
        if (!f.inco && *f.help && (!*f.alias || strcmp(it->first, f.name) == 0)) {
            if (the_first_one) {
                the_first_one = false;
                cout << "flags:\n";
            }
            f.print();
        }
    }
}

void Mod::print_all_flags() {
    cout << "flags:\n";
    for (auto it = flags.begin(); it != flags.end(); ++it) {
        const auto& f = *it->second;
        if (*f.help && (!*f.alias || strcmp(it->first, f.name) == 0)) {
            f.print();
        }
    }
}

inline void Mod::print_help() {
    cout << "usage:  " << color::blue << "$exe [-flag] [value]\n" << color::deflt
         << "\t" << "$exe -x -i 8k -s ok        # x=true, i=8192, s=\"ok\"\n"
         << "\t" << "$exe --                    # print all flags\n"
         << "\t" << "$exe -mkconf               # generate config file\n"
         << "\t" << "$exe -conf xx.conf         # run with config file\n\n";

    this->print_flags();
}

// add quotes to string if necessary
void format_str(fastring& s) {
    const size_t a = s.find_first_of("\"'`#");
    const size_t b = s.find("//");
    if (a == s.npos && b == s.npos) return;

    fastring r(std::move(s));
    if (a == s.npos || !r.contains('"')) { s << '"' << r << '"'; return; }
    if (!r.contains('\'')) { s << '\'' << r << '\''; return; }
    s << "```" << r << "```";
}

void Mod::make_config(const fastring& exe) {
    // order flags by lv, file, line
    co::map<int, co::map<const char*, co::map<int, Flag*>>> o;
    for (auto it = flags.begin(); it != flags.end(); ++it) {
        Flag* f = it->second;
        if (f->help[0] == '.' || f->help[0] == '\0') continue; // ignore hidden flags.
        o[f->lv][f->file][f->line] = f;
    }

    fastring fname(exe);
    fname.remove_suffix(".exe");
    fname += ".conf";

    fs::fstream f(fname.c_str(), 'w');
    if (!f) {
        cout << "can't open config file: " << fname << endl;
        return;
    }

    const int COMMENT_LINE_LEN = 72;
    f << fastring(COMMENT_LINE_LEN, '#') << '\n'
      << "###  > # or // for comments\n"
      << "###  > k,m,g,t,p (case insensitive, 1k for 1024, etc.)\n"
      << fastring(COMMENT_LINE_LEN, '#') << "\n\n\n";

    for (auto it = o.begin(); it != o.end(); ++it) {
        const auto& x = it->second;
        for (auto xit = x.begin(); xit != x.end(); ++xit) {
            const auto& y = xit->second;
            //f << "# >> " << str::replace(xit->first, "\\", "/") << '\n';
            f << "#" << fastring(COMMENT_LINE_LEN - 1, '=') << '\n';
            for (auto yit = y.begin(); yit != y.end(); ++yit) {
                const Flag& flag = *yit->second;
                fastring v = flag.get_value();
                if (flag.iden == 's') format_str(v);
                f << "# " << str::replace(flag.help, "\n", "\n# ") << '\n';
                f << flag.name << " = " << v << "\n\n";
            }
            f << "\n";
        }
    }

    f.flush();
}

// @kv:  for -a=b, or -a b, or a=b
// @k:   for -a, -xyz
// return non-flag elements (etc. hello, -8, -8k, -, --, --- ...)
co::vector<fastring> Mod::analyze_args(
    const co::vector<fastring>& args, co::map<fastring, fastring>& kv, co::vector<fastring>& k 
) {
    co::vector<fastring> res;

    for (size_t i = 0; i < args.size(); ++i) {
        const fastring& arg = args[i];
        size_t bp = arg.find_first_not_of('-');
        size_t ep = arg.find('=');

        // @arg has only '-':  for -, --, --- ...
        if (bp == arg.npos) {
            res.push_back(arg);
            continue;
        }

        if (ep <= bp) {
            cout << "invalid parameter" << ": " << arg << endl;
            ::exit(0);
        }

        // @arg has '=', for -a=b or a=b
        if (ep != arg.npos) {
            kv[arg.substr(bp, ep - bp)] = arg.substr(ep + 1);
            continue;
        }

        // non-flag: etc. hello, -8, -8k ...
        if (bp == 0 || (bp == 1 && '0' <= arg[1] && arg[1] <= '9')) {
            res.push_back(arg);
            continue;
        }

        // flag: -a, -a b, or -j4
        {
            Flag* f = 0;
            fastring next;
            fastring name = arg.substr(bp);

            // for -j4
            if (name.size() > 1 && (('0' <= name[1] && name[1] <= '9') || name[1] == '-')) {
                char sub[2] = { name[0], '\0' };
                if (!find_flag(name.c_str()) && find_flag(sub)) {
                    kv[name.substr(0, 1)] = name.substr(1);
                    continue;
                }
            }

            if (i + 1 == args.size()) goto no_value;

            next = args[i + 1];
            if (next.find('=') != next.npos) goto no_value;
            if (next.starts_with('-') && next.find_first_not_of('-') != next.npos) {
                if (next[1] < '0' || next[1] > '9') goto no_value;
            }

            f = find_flag(name.c_str());
            if (!f) goto no_value;
            if (f->iden != 'b') goto has_value;
            if (next == "0" || next == "1" || next == "false" || next == "true") goto has_value;

          no_value:
            k.push_back(name);
            continue;

          has_value:
            kv[name] = next;
            ++i;
            continue;
        };
    }

    return res;
}

co::vector<fastring> Mod::parse_commandline(int argc, char** argv) {
    if (argc <= 1) return co::vector<fastring>();

    co::vector<fastring> args(argc - 1);
    for (int i = 1; i < argc; ++i) args.push_back(fastring(argv[i]));

    if (args.size() == 1 && args[0] == "--") {
        this->print_all_flags();
        ::exit(0);
    }

    co::map<fastring, fastring> kv;
    co::vector<fastring> k;
    co::vector<fastring> v = this->analyze_args(args, kv, k);

    // $exe -xx    (xx is a flag of string type)
    if (v.empty() && kv.empty() && k.size() == 1) {
        const auto& name = k[0];
        auto f = this->find_flag(name.c_str());
        if (f && f->iden == 's') {
            auto& s = *static_cast<fastring*>(f->addr);
            if (!s.empty()) { cout << s << endl; ::exit(0); }
            if (name == "help") { this->print_help(); ::exit(0); }
            cout << name << ": value not set" << endl;
            ::exit(0);
        }
    }

    auto it = kv.find("config");
    if (it == kv.end()) it = kv.find("conf");
    if (it != kv.end()) {
        FLG_config = it->second;
    } else if (!v.empty()) {
        if (v[0].ends_with(".conf") || v[0].ends_with("config")) {
            if (fs::exists(v[0])) FLG_config = v[0];
        }
    }

    if (!FLG_config.empty()) this->parse_config(FLG_config);

    for (it = kv.begin(); it != kv.end(); ++it) {
        fastring e = this->set_flag_value(it->first.c_str(), it->second);
        if (!e.empty()) {
            cout << e << endl;
            ::exit(0);
        }
    }

    for (size_t i = 0; i < k.size(); ++i) {
        fastring e = this->set_bool_flags(k[i].c_str());
        if (!e.empty()) {
            cout << e << endl;
            ::exit(0);
        }
    }

    return v;
}

void remove_quotes_and_comments(fastring& s) {
    if (s.empty()) return;

    size_t p, q, l;
    char c = s[0];

    if (c == '"' || c == '\'' || c == '`') {
        if (!s.starts_with("```")) {
            p = s.find(c, 1);
            l = 1;
        } else {
            p = s.find("```", 3);
            l = 3;
        }

        if (p == s.npos) goto no_quotes;

        p = s.find_first_not_of(" \t", p + l);
        if (p == s.npos) {
            s.trim(" \t", 'r');
        } else if (s[p] == '#' || s.substr(p, 2) == "//") {
            s.resize(p);
            s.trim(" \t", 'r');
        } else {
            goto no_quotes;
        }

        s.trim(l, 'b');
        return;
    }

  no_quotes:
    p = s.find('#');
    q = s.find("//");
    if (p != s.npos || q != s.npos) {
        s.resize(p < q ? p : q);
        s.trim(" \t", 'r');
    }
}

fastring getline(co::vector<fastring>& lines, size_t& n) {
    fastring line;
    while (n < lines.size()) {
        fastring s(lines[n++]);
        s.replace("　", " ");  // replace Chinese spaces
        s.trim();
        if (s.empty() || s.back() != '\\') {
            line += s;
            return line;
        }
        line += str::trim(s, " \t\r\n\\", 'r');
    }
    return line;
}

void Mod::parse_config(const fastring& config) {
    fs::file f(config, 'r');
    if (!f) {
        cout << "can't open config file: " << config << endl;
        ::exit(0);
    }

    fastring data = f.read((size_t)f.size());
    char sep = '\n';
    if (data.find('\n') == data.npos && data.find('\r') != data.npos) sep = '\r';

    auto lines = str::split(data, sep);
    size_t lineno = 0; // line number

    for (size_t i = 0; i < lines.size();) {
        lineno = i;
        fastring s = getline(lines, i);
        if (s.empty() || s[0] == '#' || s.starts_with("//")) continue;

        size_t p = s.find('=');
        if (p == 0 || p == s.npos) {
            cout << "invalid config: " << s << ", at " << config << ':' << (lineno + 1) << endl;
            ::exit(0);
        }

        fastring flg = str::trim(s.substr(0, p), " \t", 'r');
        fastring val = str::trim(s.substr(p + 1), " \t", 'l');
        remove_quotes_and_comments(val);

        fastring e = this->set_flag_value(flg.c_str(), val);
        if (!e.empty()) {
            if (!e.starts_with("flag not defined")) {
                cout << e << ", at " << config << ':' << (lineno + 1) << endl;
                ::exit(0);
            } else {
                cout << "WARNING: " << e << ", at " << config << ':' << (lineno + 1) << endl;
            }
        }
    }
}

void add_flag(
    char iden, const char* name, const char* value, const char* help, 
    const char* file, int line, void* addr, const char* alias
) {
    mod().add_flag(iden, name, value, help, file, line, addr, alias);
}

} // namespace xx

co::vector<fastring> parse(int argc, char** argv) {
    auto& mod = xx::mod();
    auto v = mod.parse_commandline(argc, argv);
    if (FLG_mkconf) {
        mod.make_config(argv[0]);
        ::exit(0);
    }
    if (FLG_daemon) os::daemon();
    return v;
}

void parse(const fastring& path) {
    xx::mod().parse_config(path);
}

fastring set_value(const char* name, const fastring& value) {
    return xx::mod().set_flag_value(name, value);
}

fastring alias(const char* name, const char* new_name) {
    return xx::mod().alias(name, new_name);
}

} // namespace flag
