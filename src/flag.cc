#include "co/flag.h"
#include "co/cout.h"
#include "co/fs.h"
#include "co/os.h"
#include "co/str.h"

DEF_string(help, "", ">>.help info");
DEF_string(config, "", ">>.path of config file", conf);
DEF_string(version, "", ">>.version of the program");
DEF_bool(mkconf, false, ">>.generate config file");
DEF_bool(daemon, false, ">>#0 run program as a daemon");

extern void _co_set_except_handler();

namespace flag {
namespace xx {

struct Flag {
    Flag(char type, const char* name, const char* alias, const char* value, const char* help, 
         const char* file, int line, void* addr);

    fastring set_value(const fastring& v);
    fastring get_value() const;
    void print() const;
    const char* type_str() const;

    char type;
    bool inco;          // flag inside co (comment starts with >>)
    const char* name;
    const char* alias;  // alias for this flag
    const char* value;  // default value
    const char* help;   // help info
    const char* file;   // file where the flag is defined
    int line;           // line of the file where the flag is defined
    int lv;             // level: 0-9
    void* addr;         // point to the flag variable
};

inline co::map<fastring, Flag*>& gFlags() {
    static auto flags = co::static_new<co::map<fastring, Flag*>>();
    return *flags;
}

const char TYPE_string = 's';
const char TYPE_bool = 'b';
const char TYPE_int32 = 'i';
const char TYPE_int64 = 'I';
const char TYPE_uint32 = 'u';
const char TYPE_uint64 = 'U';
const char TYPE_double = 'd';

Flag::Flag(char type, const char* name, const char* alias, const char* value, 
           const char* help, const char* file, int line, void* addr)
    : type(type), inco(false), name(name), alias(alias), value(value),
      help(help), file(file), line(line), lv(5), addr(addr) {
    if (help[0] == '>' && help[1] == '>') { /* flag defined in co */
        this->inco = true;
        this->help += 2;
    }

    // get level(0-9) at the beginning of help
    const char* const h = this->help;
    if (h[0] == '#' && '0' <= h[1] && h[1] <= '9' && (h[2] == ' ' || h[2] == '\0')) {
        lv = h[1] - '0';
        this->help += 2 + !!h[2];
    }
}

fastring Flag::set_value(const fastring& v) {
    switch (this->type) {
      case TYPE_string:
        *static_cast<fastring*>(this->addr) = v;
        return fastring();
      case TYPE_bool:
        *static_cast<bool*>(this->addr) = str::to_bool(v);
        break;
      case TYPE_int32:
        *static_cast<int32*>(this->addr) = str::to_int32(v);
        break;
      case TYPE_uint32:
        *static_cast<uint32*>(this->addr) = str::to_uint32(v);
        break;
      case TYPE_int64:
        *static_cast<int64*>(this->addr) = str::to_int64(v);
        break;
      case TYPE_uint64:
        *static_cast<uint64*>(this->addr) = str::to_uint64(v);
        break;
      case TYPE_double:
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
fastring int_to_string(T t) {
    if ((0 <= t && t <= 8192) || (t < 0 && t >= -8192)) return str::from(t);

    const char* u = "kmgtp";
    int i = -1;
    while (t != 0 && (t & 1023) == 0) {
        t >>= 10;
        if (++i >= 4) break;
    }

    fastring s = str::from(t);
    if (i >= 0) s.append(u[i]);
    return s;
}

fastring Flag::get_value() const {
    switch (this->type) {
      case TYPE_string:
        return *static_cast<fastring*>(this->addr);
      case TYPE_bool:
        return str::from(*static_cast<bool*>(this->addr));
      case TYPE_int32:
        return int_to_string(*static_cast<int32*>(this->addr));
      case TYPE_uint32:
        return int_to_string(*static_cast<uint32*>(this->addr));
      case TYPE_int64:
        return int_to_string(*static_cast<int64*>(this->addr));
      case TYPE_uint64:
        return int_to_string(*static_cast<uint64*>(this->addr));
      case TYPE_double:
        return str::from(*static_cast<double*>(this->addr));
      default:
        return "unknown flag type";
    }
}

inline const char* Flag::type_str() const {
    switch (this->type) {
      case TYPE_string: return "string";
      case TYPE_bool:   return "bool";
      case TYPE_int32:  return "int32";
      case TYPE_uint32: return "uint32";
      case TYPE_int64:  return "int64";
      case TYPE_uint64: return "uint64";
      case TYPE_double: return "double";
      default:          return "unknown flag type";
    }
}

inline void Flag::print() const {
    cout << color::green << "    -" << this->name;
    if (*this->alias) cout << ", " << this->alias;
    cout.flush();
    cout << color::blue << "  " << this->help << '\n' << color::deflt
         << "\ttype: " << this->type_str()
         << "\t  default: " << this->value
         << "\n\tfrom: " << this->file
         << endl;
}

void add_flag(
    char type, const char* name, const char* value, const char* help, 
    const char* file, int line, void* addr, const char* alias) {
    auto f = co::static_new<Flag>(type, name, alias, value, help, file, line, addr);
    auto r = gFlags().insert(std::make_pair(fastring(name), f));

    if (!r.second) {
        cout << "multiple definitions of flag: " << name << ", from "
             << r.first->second->file << " and " << file << endl;
        exit(0);
    }

    if (alias[0]) {
        auto v = str::split(alias, ',');
        for (auto& x : v) {
            x.strip();
            auto r = gFlags().insert(std::make_pair(x, f));
            if (!r.second) {
                cout << "alias " << name << " as " << x << " failed, flag " << x
                     << " already exists in " << r.first->second->file << endl;
                exit(0);
            }
        }
    }
}

inline Flag* find_flag(const fastring& name) {
    auto it = gFlags().find(name);
    return it != gFlags().end() ? it->second : NULL;
}

// Return error message on any error.
fastring set_flag_value(const fastring& name, const fastring& value) {
    Flag* flag = find_flag(name);
    if (!flag) return "flag not defined: " + name;

    fastring err = flag->set_value(value);
    if (!err.empty()) err.append(": ").append(value);
    return err;
}

// set_bool_flags("abc"):  -abc -> true  or  -a, -b, -c -> true
fastring set_bool_flags(const fastring& name) {
    Flag* flag = find_flag(name);
    if (flag) {
        if (flag->type == TYPE_bool) {
            *static_cast<bool*>(flag->addr) = true;
            return fastring();
        } else {
            return fastring("value not set for non-bool flag: ").append(name);
        }
    }

    if (name.size() == 1) {
        return fastring("undefined bool flag: ").append(name);
    }

    for (size_t i = 0; i < name.size(); ++i) {
        flag = find_flag(name.substr(i, 1));
        if (!flag) {
            return fastring("undefined bool flag -") + name[i] + " in -" + name;
        } else if (flag->type != TYPE_bool) {
            return fastring("-") + name[i] + " is not bool in -" + name;
        } else {
            *static_cast<bool*>(flag->addr) = true;
        }
    }

    return fastring();
}

void show_user_flags() {
    bool the_first_one = true;
    for (auto it = gFlags().begin(); it != gFlags().end(); ++it) {
        const auto& f = *it->second;
        if (!f.inco && *f.help && (!*f.alias || it->first == f.name)) {
            if (the_first_one) {
                the_first_one = false;
                cout << "flags:\n";
            }
            f.print();
        }
    }
}

void show_co_flags() {
    cout << "flags:\n";
    for (auto it = gFlags().begin(); it != gFlags().end(); ++it) {
        const auto& f = *it->second;
        if (f.inco && *f.help && (!*f.alias || it->first == f.name)) {
            f.print();
        }
    }
}

void show_all_flags() {
    cout << "flags:\n";
    for (auto it = gFlags().begin(); it != gFlags().end(); ++it) {
        const auto& f = *it->second;
        if (*f.help && (!*f.alias || it->first == f.name)) {
            f.print();
        }
    }
}

// print FLG_help first if it is not empty, then print usage and user flags
inline void show_help() {
    if (!FLG_help.empty()) {
        cout << FLG_help << endl;
        return;
    }

    cout << "usage:  " << color::blue << "$exe [-flag] [value]\n" << color::deflt
         << "\t" << "$exe -x -i 8k -s ok        # x=true, i=8192, s=\"ok\"\n"
         << "\t" << "$exe --                    # print all flags\n"
         << "\t" << "$exe -mkconf               # generate config file\n"
         << "\t" << "$exe -conf xx.conf         # run with config file\n\n";

    show_user_flags();
}

inline void show_version() {
    if (!FLG_version.empty()) cout << FLG_version << endl;
}

fastring format_str(const fastring& s) {
    size_t px = s.find('"');
    size_t py = s.find('\'');
    size_t pz = s.find('`');
    if (px == s.npos && py == s.npos && pz == s.npos) return s;
    if (px == s.npos) return fastring(s.size() + 3).append('"').append(s).append('"');
    if (py == s.npos) return fastring(s.size() + 3).append('\'').append(s).append('\'');
    if (pz == s.npos) return fastring(s.size() + 3).append('`').append(s).append('`');
    return fastring(s.size() + 8).append("```").append(s).append("```");
}

#define COMMENT_LINE_LEN 72

void mkconf(const fastring& exe) {
    // Order flags by lv, file, line.  <lv, <file, <line, flag>>>
    co::map<int, co::map<fastring, co::map<int, Flag*>>> flags;
    for (auto it = gFlags().begin(); it != gFlags().end(); ++it) {
        Flag* f = it->second;
        if (f->help[0] == '.' || f->help[0] == '\0') continue; // ignore hidden flags.
        flags[f->lv][fastring(f->file)][f->line] = f;
    }

    fastring fname(exe);
    if (fname.ends_with(".exe")) fname.resize(fname.size() - 4);
    fname += ".conf";

    fs::fstream f(fname.c_str(), 'w');
    if (!f) {
        cout << "can't open config file: " << fname << endl;
        return;
    }

    f << fastring(COMMENT_LINE_LEN, '#') << '\n'
      << "###  > # or // for comments\n"
      << "###  > k,m,g,t,p (case insensitive, 1k for 1024, etc.)\n"
      << fastring(COMMENT_LINE_LEN, '#') << "\n\n\n";

    for (auto it = flags.begin(); it != flags.end(); ++it) {
        const auto& x = it->second;
        for (auto xit = x.begin(); xit != x.end(); ++xit) {
            const auto& y = xit->second;
            f << "# >> " << str::replace(xit->first, "\\", "/") << '\n';
            f << "#" << fastring(COMMENT_LINE_LEN - 1, '=') << '\n';
            for (auto yit = y.begin(); yit != y.end(); ++yit) {
                const Flag& flag = *yit->second;
                fastring v = flag.get_value();
                if (flag.type == TYPE_string) v = format_str(v);
                f << "# " << str::replace(flag.help, "\n", "\n# ") << '\n';
                f << flag.name << " = " << v << "\n\n";
            }
            f << "\n";
        }
    }

    f.flush();
}

#undef COMMENT_LINE_LEN

// @kv:     for -a=b, or -a b, or a=b
// @bools:  for -a, -xyz
// return non-flag elements (etc. hello, -8, -8k, -, --, --- ...)
co::vector<fastring> analyze(
    const co::vector<fastring>& args, co::map<fastring, fastring>& kv, co::vector<fastring>& bools
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
            exit(0);
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
            Flag* flag = 0;
            fastring next;
            fastring name = arg.substr(bp);

            // for -j4
            if (name.size() > 1 && (('0' <= name[1] && name[1] <= '9') || name[1] == '-')) {
                if (!find_flag(name) && find_flag(name.substr(0, 1))) {
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

            flag = find_flag(name);
            if (!flag) goto no_value;
            if (flag->type != TYPE_bool) goto has_value;
            if (next == "0" || next == "1" || next == "false" || next == "true") goto has_value;

          no_value:
            bools.push_back(name);
            continue;

          has_value:
            kv[name] = next;
            ++i;
            continue;
        };
    }

    return res;
}

void parse_config(const fastring& config);

co::vector<fastring> parse_command_line_flags(int argc, const char** argv) {
    if (argc <= 1) return co::vector<fastring>();

    co::vector<fastring> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(fastring(argv[i]));
    }

    if (args.size() == 1 && args[0].starts_with("--")) {
        const fastring& arg = args[0];
        if (arg.size() == 2) { /* arg == "--" */
            show_all_flags();
            exit(0);
        }

        if (arg == "--help") {
            show_help();
            exit(0);
        }

        if (arg == "--version") {
            show_version();
            exit(0);
        }

        if (arg == "--cocoyaxi") {
            show_co_flags();
            exit(0);
        }
    }

    co::map<fastring, fastring> kv;
    co::vector<fastring> bools;
    co::vector<fastring> v = analyze(args, kv, bools);

    auto it = kv.find("config");
    if (it == kv.end()) it = kv.find("conf");
    if (it != kv.end()) {
        FLG_config = it->second;
    } else if (!v.empty()) {
        if (v[0].ends_with(".conf") || v[0].ends_with("config")) {
            if (fs::exists(v[0])) FLG_config = v[0];
        }
    }

    if (!FLG_config.empty()) parse_config(FLG_config);

    for (it = kv.begin(); it != kv.end(); ++it) {
        fastring err = set_flag_value(it->first, it->second);
        if (!err.empty()) {
            cout << err << endl;
            exit(0);
        }
    }

    for (size_t i = 0; i < bools.size(); ++i) {
        fastring err = set_bool_flags(bools[i]);
        if (!err.empty()) {
            cout << err << endl;
            exit(0);
        }
    }

    return v;
}

fastring remove_quotes_and_comments(const fastring& s) {
    if (s.empty()) return s;

    fastring r;
    size_t p, q, l;
    char c = s[0];

    if (c == '"' || c == '\'' || c == '`') {
        if (s.starts_with("```")) {
            p = s.find("```", 3);
            l = 3;
        } else {
            p = s.find(c, 1);
            l = 1;
        }

        if (p == s.npos) goto no_quotes;

        p = s.find_first_not_of(" \t", p + l);
        if (p == s.npos) {
            r = str::strip(s, " \t", 'r');
        } else if (s[p] == '#' || s.substr(p, 2) == "//") {
            r = str::strip(s.substr(0, p), " \t", 'r');
        } else {
            goto no_quotes;
        }

        return r.substr(l, r.size() - l * 2);
    }

  no_quotes:
    p = s.find('#');
    q = s.find("//");
    if (p == s.npos && q == s.npos) return s;
    return str::strip(s.substr(0, p < q ? p : q), " \t", 'r');
}

fastring getline(co::vector<fastring>& lines, size_t& n) {
    fastring line;

    while (n < lines.size()) {
        fastring s(lines[n++]);
        s.replace("　", " ");  // replace Chinese spaces
        s.strip();

        if (s.empty() || s.back() != '\\') {
            line += s;
            return line;
        }

        line += str::strip(s, " \t\r\n\\", 'r');
    }

    return line;
}

void parse_config(const fastring& config) {
    fs::file f(config.c_str(), 'r');
    if (!f) {
        cout << "can't open config file: " << config << endl;
        exit(0);
    }

    fastring data = f.read((size_t)f.size());
    char sep = '\n';
    if (data.find('\n') == data.npos && data.find('\r') != data.npos) sep = '\r';

    auto lines = str::split(data, sep);
    size_t lineno = 0; // line number of config file.

    for (size_t i = 0; i < lines.size();) {
        lineno = i;
        fastring s = getline(lines, i);
        if (s.empty() || s[0] == '#' || s.starts_with("//")) continue;

        size_t p = s.find('=');
        if (p == 0 || p == s.npos) {
            cout << "invalid config: " << s << ", at " << config << ':' << (lineno + 1) << endl;
            exit(0);
        }

        fastring flg = str::strip(s.substr(0, p), " \t", 'r');
        fastring val = str::strip(s.substr(p + 1), " \t", 'l');
        val = remove_quotes_and_comments(val);

        fastring err = set_flag_value(flg, val);
        if (!err.empty()) {
            if (!err.starts_with("flag not defined")) {
                cout << err << ", at " << config << ':' << (lineno + 1) << endl;
                exit(0);
            } else {
                cout << "WARNING: " << err << ", at " << config << ':' << (lineno + 1) << endl;
            }
        }
    }
}

} // namespace xx

co::vector<fastring> init(int argc, const char** argv) {
    co::vector<fastring> v = xx::parse_command_line_flags(argc, argv);

    if (FLG_mkconf) {
        xx::mkconf(argv[0]);
        exit(0);
    }

    if (FLG_daemon) os::daemon();
    return v;
}

void init(const fastring& path) {
    xx::parse_config(path);
    _co_set_except_handler();
}

fastring set_value(const fastring& name, const fastring& value) {
    return xx::set_flag_value(name, value);
}

bool alias(const char* name, const char* new_name) {
    auto flag = xx::find_flag(name);
    if (!flag || !*new_name) return false;

    auto r = xx::gFlags().insert(std::make_pair(fastring(new_name), flag));
    if (!r.second) return false; // new_name already exists

    flag->alias = new_name;
    return true;
}

} // namespace flag
