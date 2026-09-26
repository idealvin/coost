#include "co/flag.h"
#include "co/defer.h"
#include "co/fs.h"
#include "co/print.h"
#include "co/stl.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> // for GetUserDefaultUILanguage
#endif


#define ss(name) name[g_lang]
#define SS(name, c, e) static const char* name[2] = { c, e };

SS(s_help, "@c 显示帮助信息", "@c show help info")
SS(s_version, "@c 显示版本信息", "@c show version")
SS(s_mkconf, "@c 生成配置文件", "@c generate config file")
SS(e_range, "超出数值范围", "out of range")
SS(e_inval, "无效数值", "invalid value")
SS(e_not_found, "未找到flag", "flag not found")
SS(e_name_used, "已用于", "already used in")
SS(e_redef, "重定义于", "redefined in")
SS(e_multi_alias, "不允许多个别名", "multiple aliases are not allowed")
SS(e_alias_conflict, "别名冲突", "alias name conflict")
SS(e_no_value, "值未设置", "value not set")
SS(e_open_failed, "打开文件失败", "open file failed")
SS(e_conf, "无效配置", "invalid config")
SS(e_quote, "引号缺失", "quote missing")
SS(e_badstr, "无效字符串值", "invalid string value")

DEF_bool(help, false, s_help);
DEF_bool(version, false, s_version);
DEF_bool(mkconf, false, s_mkconf);

static bool g_command_line_only = false;
static int g_lang = -1;


namespace flag {
namespace xx {

struct Flag;
struct Mod {
    Mod() = default;
    ~Mod() = default;

    typedef void(*parse_cb_t)();

    void add_flag(Flag* f);
    Flag* find_flag(const char* name);

    void alias(const char* name, const char* new_name);
    co::string set_flag_attr(const char* name, char a);
    co::string set_flag_value(const char* name, const char* value);
    co::string set_bool_flags(const char* name);

    void set_config_path(const char* path) { _config_path = path; }
    void set_program_version(const char* ver) { _version = ver; }
    void add_parse_cb(parse_cb_t cb, char c) { _cbs[c != 'a'].push_back(cb); }
    void run_parse_cb(char c) {
        auto& cbs = _cbs[c != 'a'];
        if (!cbs.empty()) {
            for (auto& cb : cbs) cb();
            co::vector<parse_cb_t>().swap(cbs);
        }
    }

    void print_help(const co::string& exe);
    void make_config(const co::string& exe);
    void parse_config(const co::string& config);
    co::vector<co::string> parse_commandline(int argc, char** argv);

    co::vector<co::string> analyze_args(
        const co::vector<co::string>& args, co::map<co::string, co::string>& kv,
        co::vector<co::string>& bools
    );

    co::map<const char*, Flag*> _flags;
    co::vector<parse_cb_t> _cbs[2];
    co::string _config_path;
    co::string _version;
};

static Mod* g_mod;

inline Mod& mod() {
    return g_mod ? *g_mod : *(g_mod = co::_make_static<Mod>());
}

// flag attributes
enum _attr_t {
    attr_default = 'd',      // support both command-line and config file
    attr_command_line = 'c', // support command-line only
    attr_hidden = 'h',       // hidden, support neither
};

struct Flag {
    const char* get_help() const;
    const char* set_value(const char* s);
    co::string get_value() const;
    void print(size_t m, size_t n) const;

    char iden;
    char attr;
    bool inco; // defined in coost
    int n;
    const char* name;
    const char* alias;
    const char* value; // default value
    const char* help;
    const char* file;
    int line;
    void* addr;
};

const char* Flag::get_help() const {
    const char* h = help;
    if (*h == '@') {
        const char c = *(h + 1);
        if (c == 'i' || c == 'd' || c == 'c' || c == 'h') {
            h += 2;
            while (*h && *h == ' ') ++h;
        }
    }
    return h;
}

const char* Flag::set_value(const char* s) {
    int err = 0;
    switch (this->iden) {
        case 's':
            *static_cast<co::string*>(this->addr) = s;
            break;
        case 'b':
            *static_cast<bool*>(this->addr) = co::stob(s, &err);
            break;
        case 'i':
            *static_cast<int32*>(this->addr) = co::stoi32(s, &err);
            break;
        case 'u':
            *static_cast<uint32*>(this->addr) = co::stou32(s, &err);
            break;
        case 'I':
            *static_cast<int64*>(this->addr) = co::stoi64(s, &err);
            break;
        case 'U':
            *static_cast<uint64*>(this->addr) = co::stou64(s, &err);
            break;
        case 'd':
            *static_cast<double*>(this->addr) = co::stod(s, &err);
            break;
    }

    switch (err) {
        case 0:
            return "";
        case ERANGE:
            return ss(e_range);
        default:
            return ss(e_inval);
    }
}

template<typename T>
co::string int2str(T t) {
    int i = -1;
    if (t > 8192 || (t < 0 && t < -8192)) {
        while (t != 0 && (t & 1023) == 0) {
            t >>= 10;
            if (++i == 4) break;
        }
    }
    co::string s = co::to_string(t);
    if (i >= 0) s.append("kmgtp"[i]);
    return s;
}

co::string Flag::get_value() const {
    switch (this->iden) {
        case 's':
            return *static_cast<co::string*>(this->addr);
        case 'b':
            return co::to_string(*static_cast<bool*>(this->addr));
        case 'i':
            return int2str(*static_cast<int32*>(this->addr));
        case 'u':
            return int2str(*static_cast<uint32*>(this->addr));
        case 'I':
            return int2str(*static_cast<int64*>(this->addr));
        case 'U':
            return int2str(*static_cast<uint64*>(this->addr));
        case 'd':
            return co::to_string(*static_cast<double*>(this->addr));
        default:
            return co::string();
    }
}

void Flag::print(size_t m, size_t n) const {
    if (attr == attr_hidden) return;

    co::color c;
    auto& f = *this;
    co::print(c.bold, c.green, "  -", f.name);
    if (*f.alias) co::print(',', f.alias);
    co::print(c.deflt).flush();

    const char* h = f.get_help();
    if (n < m) co::print(co::string(m - n, ' '));
    if (n <= m) {
        co::print(
            c.bold, c.yellow, "  ", f.iden, "  ", c.deflt, h,
            c.bold, c.blue, "  (", f.get_value(), ')', c.deflt, '\n'
        );
    } else {
        co::print(
            '\n', co::string(m, ' '),
            c.bold, c.yellow, "  ", f.iden, "  ", c.deflt, h,
            c.bold, c.blue, "  (", f.get_value(), ')', c.deflt, '\n'
        );
    }
}

void Mod::add_flag(Flag* f) {
    auto r = _flags.emplace(f->name, f);
    if (!r.second) {
        auto& g = r.first->second;
        co::print(
            "flag ", f->name, ' ', ss(e_redef), ": ", 
            g->file, ':', g->line, ", ", f->file, ':', f->line, '\n'
        ).flush();
        ::exit(0);
    }

    const char* const a = f->alias;
    if (*a) {
        if (strchr(a, ',') != NULL) {
            co::print(
                ss(e_multi_alias), ", ", f->file, ':', f->line, '\n'
            ).flush();
            ::exit(0);
        }
        auto r = _flags.emplace(a, f);
        if (!r.second) {
            auto& g = r.first->second;
            co::print(
                ss(e_alias_conflict), ": ", f->file, ':', f->line, ", ",
                g->file, ':', g->line, '\n'
            ).flush();
            ::exit(0);
        }
    }
}

inline Flag* Mod::find_flag(const char* name) {
    auto it = _flags.find(name);
    return it != _flags.end() ? it->second : NULL;
}

void Mod::alias(const char* name, const char* new_name) {
    auto f = this->find_flag(name);
    if (!f) {
        co::println("flag::alias error: ", ss(e_not_found), ": ", name);
        return;
    }

    if (!new_name || !*new_name) {
        if (*f->alias) {
            _flags.erase(f->alias);
            f->alias = "";
        }
        return;
    }

    if (strcmp(f->alias, new_name) == 0) return;

    auto r = _flags.emplace(new_name, f);
    if (!r.second) {
        auto& g = r.first->second;
        co::println(
            "flag::alias error: ", new_name, ' ', ss(e_name_used),
            " flag ", g->name, '(', g->file, ':', g->line, ')'
        );
        return;
    }

    if (*f->alias) _flags.erase(f->alias);
    f->alias = new_name;
}

co::string Mod::set_flag_attr(const char* name, char a) {
    co::string e;
    Flag* f = this->find_flag(name);
    if (f) {
        f->attr = a;
    } else {
        e.cat(ss(e_not_found), ": ", name);
    }
    return e;
}

co::string Mod::set_flag_value(const char* name, const char* value) {
    co::string e;
    Flag* f = this->find_flag(name);
    if (f) {
        const char* s = f->set_value(value);
        if (*s) e.cat(s, ": ", value);
    } else {
        e.cat(ss(e_not_found), ": ", name);
    }
    return e;
}

// set_bool_flags("abc"):  -abc -> true  or  -a, -b, -c -> true
co::string Mod::set_bool_flags(const char* name) {
    co::string e;
    Flag* f = this->find_flag(name);
    if (f) {
        if (f->iden == 'b') {
            *static_cast<bool*>(f->addr) = true;
        } else {
            e.cat("flag ", name, ", ", ss(e_no_value));
        }
        return e;
    }

    const size_t n = strlen(name);
    if (n == 1) {
        e.cat(ss(e_not_found), ": ", name);
        return e;
    }

    char sub[2] = { 0 };
    for (size_t i = 0; i < n; ++i) {
        sub[0] = name[i];
        f = this->find_flag(sub);
        if (f && f->iden == 'b') {
            *static_cast<bool*>(f->addr) = true;
            continue;
        }
        e.cat(ss(e_not_found), ": ", name);
        return e;
    }

    return e;
}

void Mod::print_help(const co::string& exe) {
    co::color c;
    co::print(c.bold, "usage:  ", c.cyan, exe);
    if (!g_command_line_only) co::print(" [", exe, ".conf]");
    co::print(" [-flag [value]] [-flag=value]...\n\n", c.deflt);

    size_t m = 0;
    Flag* ff[3] = { 0 };
    for (auto it = _flags.begin(); it != _flags.end(); ++it) {
        auto& f = *(it->second);
        size_t n = strlen(f.name) + 3;
        if (*f.alias) n += strlen(f.alias) + 1;
        f.n = (int)n;
        if (n <= 21 && m < n) m = n;

        if (f.addr == &FLG_help) {
            ff[0] = &f;
        } else if (f.addr == &FLG_version) {
            ff[1] = &f;
        } else if (f.addr == &FLG_mkconf) {
            ff[2] = &f;
        }
    }

    co::print(
        c.bold, "flags:  -name[,alias]  type  comments  (default value)\n",
        c.deflt
    );

    ff[0]->print(m, ff[0]->n);
    ff[1]->print(m, ff[1]->n);
    if (!g_command_line_only) ff[2]->print(m, ff[2]->n);
    co::print().flush();

    for (auto it = _flags.begin(); it != _flags.end(); ++it) {
        auto& f = *(it->second);
        if (f.inco) {
            if (&f == ff[0] || &f == ff[1] || &f == ff[2]) continue;
            if (*f.alias && strcmp(it->first, f.name) != 0) continue;
            f.print(m, f.n);
        }
    }

    int i = 0;
    for (auto it = _flags.begin(); it != _flags.end(); ++it) {
        auto& f = *(it->second);
        if (!f.inco) {
            if (&f == ff[0] || &f == ff[1] || &f == ff[2]) continue;
            if (*f.alias && strcmp(it->first, f.name) != 0) continue;
            if (i++ == 0) co::print('\n').flush();
            f.print(m, f.n);
        }
    }
    co::print().flush();
}

// add quotes to string if necessary
inline void format_str(co::string& s) {
    if (s.find_first_of("\"'`#") != s.npos) {
        co::string r(std::move(s));
        if (!r.contains('"')) {
            s << '"' << r << '"';
        } else if (!r.contains('\'')) {
            s << '\'' << r << '\'';
        } else {
            s << "```" << r << "```";
        }
    }
}

void Mod::make_config(const co::string& exe) {
    int flag_num = 0;
    co::map<const char*, co::map<int, Flag*>> o;
    for (auto it = _flags.begin(); it != _flags.end(); ++it) {
        auto& f = *it->second;
        if (f.attr == attr_default) {
            o[f.file][f.line] = &f;
            ++flag_num;
        }
    }

    const int COMMENT_LINE_LEN = 72;
    co::string s(flag_num * 64);

    s.append(COMMENT_LINE_LEN, '#').append('\n');
    s.cat(
        "###  > # for comments\n",
        "###  > k,m,g,t,p (8k for 8192, etc.)\n"
    );
    s.append(COMMENT_LINE_LEN, '#').append(3, '\n');

    for (auto it = o.begin(); it != o.end(); ++it) {
        s.append('#').append(COMMENT_LINE_LEN - 1, '=').append('\n');
        const auto& x = it->second;
        for (auto kt = x.begin(); kt != x.end(); ++kt) {
                auto& flag = *(kt->second);
                co::string v = flag.get_value();
                if (flag.iden == 's') v.escape();
                auto h = flag.get_help();
                s.cat("# ", co::replace(h, "\n", "\n# "), '\n', flag.name, " = ");
                if (!v.contains('\\')) {
                    s.cat(v, "\n\n");
                } else {
                    s.cat('"', v, '"', "\n\n");
                }
        }
        s.append('\n');
    }

    co::string fname(exe);
    fname.remove_suffix(".exe");
    fname += ".conf";

    fs::file f(fname.c_str(), 'w');
    if (!f) {
        co::print(ss(e_open_failed), ": ", fname, '\n').flush();
        return;
    }
    f.write(s);
    f.close();
}

inline bool is_valid_identifier_start(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '_');
}

// @kv:  for -key value, or -key=value
// @k:   for -a, -xyz
// return non-flag elements (etc. hello, -8, -8k, -, --, ---, -=x...)
co::vector<co::string> Mod::analyze_args(
    const co::vector<co::string>& args, co::map<co::string, co::string>& kv, co::vector<co::string>& k 
) {
    co::vector<co::string> res;

    for (size_t i = 0; i < args.size(); ++i) {
        const co::string& arg = args[i];
        const size_t p = arg.find_first_not_of('-');
        if (p == 0 || p == arg.npos || !is_valid_identifier_start(arg[p])) {
            res.push_back(arg);
            continue;
        }

        // -a=b
        {
            const size_t e = arg.find('=', p + 1);
            if (e != arg.npos) {
                kv[arg.substr(p, e - p)] = arg.substr(e + 1);
                continue;
            }
        }

        // flag: -a, -a b, or -j4
        {
            Flag* f = 0;
            co::string next;
            co::string name = arg.substr(p);

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
            if (next.starts_with('-')) {
                const size_t x = next.find_first_not_of('-');
                if (x != next.npos && is_valid_identifier_start(next[x])) goto no_value;
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

inline co::string _exename(const char* path) {
    const char* x = strrchr(path, '/');
    if (!x) x = strrchr(path, '\\');
    return x ? co::string(x + 1) : co::string(path);
}

co::vector<co::string> Mod::parse_commandline(int argc, char** argv) {
    defer(
        this->_version.reset();
        this->_config_path.reset();
    );

    if (argc <= 1) return co::vector<co::string>();

    co::vector<co::string> args;
    args.reserve(argc - 1);
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);

    co::string exe = _exename(argv[0]);
    exe.remove_suffix(".exe");

    co::map<co::string, co::string> kv;
    co::vector<co::string> k;
    co::vector<co::string> v = this->analyze_args(args, kv, k);

    if (v.empty() && kv.empty() && k.size() == 1) {
        const auto& name = k[0];
        auto f = this->find_flag(name.c_str());
        if (f) {
            if (strcmp(f->name, "help") == 0) {
                this->print_help(exe);
                ::exit(0);
            }
            if (strcmp(f->name, "version") == 0) {
                if (!_version.empty()) {
                    co::print(_version, '\n').flush();
                } else {
                    co::print("version not set\n").flush();
                }
                ::exit(0);
            }
        }
    }

    if (!g_command_line_only) {
        if (!v.empty() && v[0].ends_with(".conf")) _config_path = v[0];
        if (!_config_path.empty()) this->parse_config(_config_path);
    }

    for (auto it = kv.begin(); it != kv.end(); ++it) {
        co::string e = this->set_flag_value(it->first.c_str(), it->second.c_str());
        if (!e.empty()) {
            co::print(e, '\n').flush();
            ::exit(0);
        }
    }

    for (size_t i = 0; i < k.size(); ++i) {
        co::string e = this->set_bool_flags(k[i].c_str());
        if (!e.empty()) {
            co::print(e, '\n').flush();
            ::exit(0);
        }
    }

    return v;
}

const char* remove_quotes_and_comments(co::string& s) {
    if (s.empty()) return "";

    size_t p;
    const char c = s[0];

    if (c == '"' || c == '\'') {
        p = s.rfind(c);
        if (p == 0) return ss(e_quote);

        p = s.find_first_not_of(" \t", p + 1);
        if (p == s.npos) {
            s.trim_right(" \t");
        } else if (s[p] == '#') {
            s.resize(p);
            s.trim_right(" \t");
        } else {
            return ss(e_badstr);
        }

        s.remove_outer(1);
        return "";
    }

    p = s.find('#');
    if (p != s.npos) {
        s.resize(p);
        s.trim_right(" \t");
    }
    return "";
}

co::string getline(co::vector<co::string>& lines, size_t& n) {
    co::string line;
    while (n < lines.size()) {
        auto& x = lines[n++];
        x.replace("　", " ");  // replace Chinese spaces
        x.trim();
        if (!x.empty() && !x.starts_with('#')) {
            if (!x.ends_with('\\')) {
                line += x;
                return line;
            }
            x.resize(x.size() - 1);
            x.trim_right(" \t\r\n");
            line += x;
        } else {
            if (!line.empty()) return line;
        }
    }
    return line;
}

void Mod::parse_config(const co::string& config) {
    fs::file f(config, 'r');
    if (!f) {
        co::print(ss(e_open_failed), ": ", config, '\n').flush();
        ::exit(0);
    }

    co::string data = f.read((size_t)f.size());
    char sep = '\n';
    if (data.find('\n') == data.npos && data.find('\r') != data.npos) sep = '\r';

    auto lines = co::split(data, sep);
    size_t lineno = 0;

    for (size_t i = 0; i < lines.size();) {
        lineno = i;
        co::string s = getline(lines, i);
        if (s.empty()) continue;

        size_t p = s.find('=');
        if (p == 0 || p == s.npos) {
            co::print(ss(e_conf), ": ", s, "  (", config, ':', lineno + 1, ')', '\n').flush();
            ::exit(0);
        }

        co::string flg = co::trim_right(s.substr(0, p), " \t");
        co::string val = co::trim_left(s.substr(p + 1), " \t");
        const char* e = remove_quotes_and_comments(val);
        if (*e) {
            co::print(e, "  (", config, ':', lineno + 1, ')', '\n').flush();
            ::exit(0);
        }

        val.unescape();
        if (!this->find_flag(flg.c_str())) {
            co::print(
                co::color::yellow("WARNING: "), ss(e_not_found), ": ", flg,
                "  (", config, ':', lineno + 1, ')', '\n'
            ).flush();
        } else {
            co::string e = this->set_flag_value(flg.c_str(), val.c_str());
            if (!e.empty()) {
                co::print(e, "  (", config, ':', lineno + 1, ')', '\n').flush();
                ::exit(0);
            }
        }
    }
}

Flag* create_flag(
    char iden, const char* name, const char* value, const char* help, 
    const char* file, int line, void* addr, const char* alias
) {
    auto f = co::_make_static<Flag>();
    f->iden = iden;
    f->attr = (char)attr_default;
    f->inco = false;
    f->name = name;
    f->alias = alias;
    f->value = value;
    f->help = help;
    f->file = file;
    f->line = line;
    f->addr = addr;

    const char* h = f->help;
    if (*h == '@') {
        const char c = *(h + 1);
        switch (c) {
            case 'i':
                f->attr = 'h';
                f->inco = true;
                break;
            case 'c':
            case 'h':
                f->attr = c;
                break;
        }
    }

    return f;
}

FlagSaver::FlagSaver(
    char iden, const char* name, const char* value, const char* help, 
    const char* file, int line, void* addr, const char* alias
) {
    auto f = create_flag(iden, name, value, help, file, line, addr, alias);
    mod().add_flag(f);
}

FlagSaver::FlagSaver(
    char iden, const char* name, const char* value, const char** help, 
    const char* file, int line, void* addr, const char* alias) {
    if (g_lang < 0) {
        g_lang = []() {
        #ifdef _WIN32
            LANGID langId = GetUserDefaultUILanguage();
            return PRIMARYLANGID(langId) == LANG_CHINESE ? 0 : 1;
        #else
            const char* p = ::getenv("LC_ALL");
            if (!p || !*p) p = ::getenv("LC_MESSAGES");
            if (!p || !*p) p = ::getenv("LANG");
            return (p && *p && ::strstr(p, "zh") != NULL) ? 0 : 1;
        #endif
        }();
    }
    auto f = create_flag(iden, name, value, help[g_lang], file, line, addr, alias);
    mod().add_flag(f);
}

} // namespace xx

void alias(const char* name, const char* new_name) {
    xx::mod().alias(name, new_name);
}

void set_config_path(const char* path) {
    xx::mod().set_config_path(path);
}

void set_program_version(const char* ver) {
    xx::mod().set_program_version(ver);
}

void hide(const char* name) {
    auto e = xx::mod().set_flag_attr(name, xx::attr_hidden);
    if (!e.empty()) co::println("flag::hide error: ", e);
}

void unhide(const char* name) {
    auto e = xx::mod().set_flag_attr(name, xx::attr_default);
    if (!e.empty()) co::println("flag::unhide error: ", e);
}

void set_value(const char* name, const char* value) {
    auto e = xx::mod().set_flag_value(name, value);
    if (!e.empty()) co::println("flag::set_value error: ", e);
}

void run_after_parse(void(*cb)()) {
    xx::mod().add_parse_cb(cb, 'a');
}

// add a callback to be called before command line args are parsed
void run_before_parse(void(*cb)()) {
    xx::mod().add_parse_cb(cb, 'b');
}

co::vector<co::string> parse(int argc, char** argv, bool command_line_only) {
    auto& mod = xx::mod();
    mod.run_parse_cb('b');

    g_command_line_only = command_line_only;
    auto v = mod.parse_commandline(argc, argv);
    if (FLG_mkconf) {
        mod.make_config(argv[0]);
        ::exit(0);
    }

    mod.run_parse_cb('a');
    return v;
}

} // flag

#undef SS
#undef ss
