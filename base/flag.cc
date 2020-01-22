#include "flag.h"
#include "log.h"
#include "fs.h"
#include "os.h"
#include "str.h"

#include <stdlib.h>
#include <map>

DEF_string(config, "", ".path of config file");
DEF_bool(mkconf, false, ".generate config file");
DEF_bool(daemon, false, "#0 run program as a daemon");

namespace flag {
namespace xx {

struct Flag {
    Flag(const char* type_str_, const char* name_, const char* value_,
         const char* help_, const char* file_, int line_, int type_, void* addr_)
        : type_str(type_str_), name(name_), value(value_), help(help_),
          file(file_), line(line_), type(type_), addr(addr_), lv(10) {
        const char* const h = help;
        if (h[0] == '#' && '0' <= h[1] && h[1] <= '9') {
            if (h[2] == ' ' || h[2] == '\0') {
                lv = h[1] - '0';
                help += 2 + !!h[2];
            } else if ('0' <= h[2] && h[2] <= '9' && (h[3] == ' ' || h[3] == '\0')) {
                lv = (h[1] - '0') * 10 + (h[2] - '0');
                help += 3 + !!h[3];
            }
        }
    }

    const char* type_str;
    const char* name;
    const char* value;  // default value
    const char* help;   // help info
    const char* file;   // file where the flag is defined
    int line;           // line of the file where the flag is defined
    int type;
    void* addr;         // point to the flag variable
    int lv;             // level: 0-9 for co, 10-99 for users
};

inline std::map<fastring, Flag>& gFlags() {
    static std::map<fastring, Flag> flags; // <name, flag>
    return flags;
}

void flag_set_value(Flag* flag, const fastring& v) {
    switch (flag->type) {
      case TYPE_string:
        *static_cast<fastring*>(flag->addr) = v;
        break;
      case TYPE_bool:
        *static_cast<bool*>(flag->addr) = str::to_bool(v);
        break;
      case TYPE_int32:
        *static_cast<int32*>(flag->addr) = str::to_int32(v);
        break;
      case TYPE_uint32:
        *static_cast<uint32*>(flag->addr) = str::to_uint32(v);
        break;
      case TYPE_int64:
        *static_cast<int64*>(flag->addr) = str::to_int64(v);
        break;
      case TYPE_uint64:
        *static_cast<uint64*>(flag->addr) = str::to_uint64(v);
        break;
      case TYPE_double:
        *static_cast<double*>(flag->addr) = str::to_double(v);
        break;
      default:
        throw "unknown flag type";
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

    return i < 0 ? str::from(t) : str::from(t) + u[i];
}

fastring flag_get_value(const Flag* flag) {
    switch (flag->type) {
      case TYPE_string:
        return *static_cast<fastring*>(flag->addr);
      case TYPE_bool:
        return str::from(*static_cast<bool*>(flag->addr));
      case TYPE_int32:
        return int_to_string(*static_cast<int32*>(flag->addr));
      case TYPE_uint32:
        return int_to_string(*static_cast<uint32*>(flag->addr));
      case TYPE_int64:
        return int_to_string(*static_cast<int64*>(flag->addr));
      case TYPE_uint64:
        return int_to_string(*static_cast<uint64*>(flag->addr));
      case TYPE_double:
        return str::from(*static_cast<double*>(flag->addr));
      default:
        throw "unknown flag type";
    }
}

fastring flag_to_str(const Flag* flag) {
    return fastring(64).append("--").append(flag->name)
                       .append(": ").append(flag->help)
                       .append("\n\t type: ").append(flag->type_str)
                       .append("\t     default: ").append(flag->value)
                       .append("\n\t from: ").append(flag->file);
}

void add_flag(const char* type_str, const char* name, const char* value,
              const char* help, const char* file, int line, int type, void* addr) {
    auto r = gFlags().insert(
        std::make_pair(fastring(name), Flag(type_str, name, value, help, file, line, type, addr))
    );

    if (!r.second) {
        COUT << "multiple definitions of flag: " << name
             << ", from " << r.first->second.file << " and " << file;
        exit(0);
    }
}

Flag* find_flag(const fastring& name) {
    auto it = gFlags().find(name);
    if (it != gFlags().end()) return &it->second;
    return NULL;
}

// Return error message on any error.
fastring set_flag_value(const fastring& name, const fastring& value) {
    Flag* flag = find_flag(name);
    if (!flag) return "flag not defined: " + name;

    try {
        flag_set_value(flag, value);
        return fastring();
    } catch (const char* s) {
        return fastring(s) + ": " + value;
    }
}

// set_bool_flags("abc"):  -abc -> true  or  -a, -b, -c -> true
fastring set_bool_flags(const fastring& name) {
    Flag* flag = find_flag(name);
    if (flag) {
        if (flag->type == TYPE_bool) {
            *static_cast<bool*>(flag->addr) = true;
            return fastring();
        } else {
            return fastring("value not set for non-bool flag: -").append(name);
        }
    }

    if (name.size() == 1) {
        return fastring("undefined bool flag -").append(name);
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

void show_flags_info() {
    for (auto it = gFlags().begin(); it != gFlags().end(); ++it) {
        const auto& flag = it->second;
        if (*flag.help != '\0') COUT << flag_to_str(&flag);
    }
}

void show_help_info(const fastring& exe) {
    fastring s(exe);
  #ifdef _WIN32
    if (s.size() > 1 && s[1] == ':') {
        size_t p = s.rfind('/');
        if (p == s.npos) p = s.rfind('\\');
        if (p != s.npos) s = "./" + s.substr(p + 1);
    }
  #endif
    COUT << "usage:\n"
         << "    " << s << " --                   print flags info\n"
         << "    " << s << " --help               print this help info\n"
         << "    " << s << " --mkconf             generate config file\n"
         << "    " << s << " --daemon             run as a daemon (Linux)\n"
         << "    " << s << " xx.conf              run with config file\n"
         << "    " << s << " config=xx.conf       run with config file\n"
         << "    " << s << " -x -i=8k -s=ok       run with commandline flags\n"
         << "    " << s << " -x -i 8k -s ok       run with commandline flags\n"
         << "    " << s << " x=true i=8192 s=ok   run with commandline flags\n";
}

fastring format_str(const fastring& s) {
    size_t px = s.find('"');
    size_t py = s.find('\'');
    if (px == s.npos && py == s.npos) return s;
    if (px == s.npos) return fastring(s.size() + 3).append('"').append(s).append('"');
    if (py == s.npos) return fastring(s.size() + 3).append('\'').append(s).append('\'');
    return fastring(s.size() + 8).append('\'').append(str::replace(s, "'", "\\'")).append('\'');
}

#define COMMENT_LINE_LEN 72

void mkconf(const fastring& exe) {
    // Order flags by lv, file, line.  <lv, <file, <line, flag>>>
    std::map<int, std::map<fastring, std::map<int, Flag*>>> flags;
    for (auto it = gFlags().begin(); it != gFlags().end(); ++it) {
        Flag* f = &it->second;
        if (f->help[0] == '.' || f->help[0] == '\0') continue; // ignore hidden flags.
        flags[f->lv][fastring(f->file)][f->line] = f;
    }

    fastring fname(exe.clone());
    if (fname.ends_with(".exe")) fname.resize(fname.size() - 4);
    fname += ".conf";

    fs::fstream f(fname.c_str(), 'w');
    if (!f) {
        COUT << "can't open config file: " << fname;
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
                fastring v = flag_get_value(&flag);
                if (flag.type == TYPE_string) v = format_str(v);
                f << "# " << str::replace(flag.help, "\n", "\n# ") << '\n';
                f << flag.name << " = " << v << "\n\n";
            }
            f << "\n";
        }
    }

    f.flush();
}


FlagSaver::FlagSaver(const char* type_str, const char* name, const char* value,
                     const char* help, const char* file, int line, int type, void* addr) {
    add_flag(type_str, name, value, help, file, line, type, addr);
}

// @kv:     for -a=b, or -a b, or a=b
// @bools:  for -a, -xyz
// return non-flag elements (etc. hello, -8, -8k, -, --, --- ...)
std::vector<fastring> analyze(
    const std::vector<fastring>& args, std::map<fastring, fastring>& kv, std::vector<fastring>& bools
) {
    std::vector<fastring> res;

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
            COUT << "invalid parameter" << ": " << arg;
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
            if (next == "0" || next == "1" && next == "false" && next == "true") goto has_value;

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

std::vector<fastring> parse_command_line_flags(int argc, char** argv) {
    if (argc <= 1) return std::vector<fastring>();

    std::vector<fastring> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(fastring(argv[i]));
    }

    fastring exe(argv[0]);

    if (args.size() == 1) {
        const fastring& arg = args[0];

        if (arg == "--help") {
            show_help_info(exe);
            exit(0);
        }

        if (arg.find_first_not_of('-') == arg.npos) {
            show_flags_info();
            exit(0);
        }
    }

    std::map<fastring, fastring> kv;
    std::vector<fastring> bools;
    std::vector<fastring> v = analyze(args, kv, bools);

    auto it = kv.find("config");
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
            COUT << err;
            exit(0);
        }
    }

    for (size_t i = 0; i < bools.size(); ++i) {
        fastring err = set_bool_flags(bools[i]);
        if (!err.empty()) {
            COUT << err;
            exit(0);
        }
    }

    return v;
}

size_t find_not_in_qm(const char* s, const char* sub) {
    if (!s || *s == '\0') return (size_t)-1;

    char x = 0, y = 0;
    const char* i = s;
    const char* e = s + strlen(s);

    while (true) {
        const char* p = strstr(i, sub);
        if (p == 0) return (size_t)-1;

        for (; i <= p; ++i) {
            if (*i != '"' && *i != '\'') continue;

            if (*i == x) {
                x = y = 0;
            } else if (*i == y) {
                y = 0;
            } else if (x == 0) {
                x = *i;
            } else {  /* y == 0 */
                y = *i;
            }
        }

        if (x == 0) return p - s; // left qm not found before p

        for (i = p + strlen(sub); i < e; ++i) {
            if (*i != '"' && *i != '\'') continue;

            if (*i == x || *i == y) {
                x = y = 0;
                break;
            }
        }

        if (i++ == e) return p - s;
    }
}

fastring getline(std::vector<fastring>& lines, size_t& n) {
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
        COUT << "can't open config file: " << config;
        exit(0);
    }

    fastring data = f.read(f.size());

    char sep = '\n';
    if (data.find('\n') == data.npos && data.find('\r') != data.npos) sep = '\r';

    auto lines = str::split(data, sep);
    size_t lineno = 0; // line number of config file.

    for (size_t i = 0; i < lines.size();) {
        lineno = i;
        fastring s = getline(lines, i);
        if (s.empty() || s[0] == '#' || s.starts_with("//")) continue;

        // remove tailing comments.
        size_t p = find_not_in_qm(s.c_str(), "#");
        size_t q = find_not_in_qm(s.c_str(), "//");
        if (p > q) p = q;
        if (p != s.npos) s = str::strip(s.substr(0, p), " \t", 'r');

        p = find_not_in_qm(s.c_str(), "=");
        if (p == 0 || p == s.npos) {
            COUT << "invalid config: " << s << ", at " << config << ':' << (lineno + 1);
            exit(0);
        }

        fastring flg = str::strip(s.substr(0, p), " \t", 'r');
        fastring val = str::strip(s.substr(p + 1), " \t", 'l');

        if (val.size() > 1) {
            char c = val.front();
            if ((c == '"' || c == '\'') && c == val.back()) {
                val.strip(c);
            }
        }

        fastring err = set_flag_value(flg, val);
        if (!err.empty()) {
            COUT << err << ", at " << config << ':' << (lineno + 1);
            exit(0);
        }
    }
}

} // namespace xx

std::vector<fastring> init(int argc, char** argv) {
    std::vector<fastring> v = xx::parse_command_line_flags(argc, argv);

    if (FLG_mkconf) {
        xx::mkconf(argv[0]);
        exit(0);
    }

    if (FLG_daemon) os::daemon();
    return v;
}

} // namespace flag
