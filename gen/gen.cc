#include "gen.h"
#include "co/def.h"
#include "co/defer.h"
#include "co/str.h"
#include "co/fs.h"
#include "co/flag.h"
#include "co/json.h"
#include "co/cout.h"

DEF_bool(std, false, "use std types in the generated code");
 
void yyerror(const char* s) {
    cout << s << " at line " << yylineno << ", last token: " << yytext << endl;
}

int g_us = 0;
int g_uv = 0;
Program* g_prog = 0;
fastring g_sname;
fastring g_aname;

inline fastring indent(int n) { return fastring(n, ' '); }

void gen_service(fs::fstream& fs, const fastring& serv, const co::vector<fastring>& methods) {
    // class for service
    fs << "class " << serv << " : public rpc::Service {\n";
    fs << "  public:\n";
    fs << indent(4) << "typedef std::function<void(co::Json&, co::Json&)> Fun;\n\n";

    do {
        fs << indent(4) << serv << "() {\n";
        fs << indent(8) << "using std::placeholders::_1;\n";
        fs << indent(8) << "using std::placeholders::_2;\n";
        for (size_t i = 0; i < methods.size(); ++i) {
            fs << indent(8) << "_methods[\"" << serv << '.' << methods[i] << "\"] = "
               << "std::bind(&" << serv << "::" << methods[i] << ", this, _1, _2);\n";
        }
        fs << indent(4) << "}\n\n";

        fs << indent(4) << "virtual ~" << serv << "() {}\n\n";
    } while (0);

    // virtual const char* name() const
    fs << indent(4) << "virtual const char* name() const {\n"
       << indent(8) << "return \"" << serv << "\";\n"
       << indent(4) << "}\n\n";

    fs << indent(4) << "virtual const co::map<const char*, Fun>& methods() const {\n"
       << indent(8) << "return _methods;\n"
       << indent(4) << "}\n\n";

    // virtual void xxx(co::Json& req, co::Json& res)
    for (size_t i = 0; i < methods.size(); ++i) {
        fs << indent(4) << "virtual void " << methods[i] << "(co::Json& req, co::Json& res) = 0;\n\n";
    }

    fs << "  private:\n";
    fs << "    co::map<const char*, Fun> _methods;\n";
    fs << "};\n\n";
}

fastring array_proto(Array* a) {
    fastring s(32);
    Type* e = a->element_type();
    s << g_aname;
    if (e->type() == type_string) {
        s << '<' << g_sname << '>';
    } else if (e->type() != type_array) {
        s << '<' << e->name() << '>';
    } else {
        s << '<' << array_proto((Array*)e) << '>';
    }
    return s;
}

inline fastring unamed_var() {
    fastring s(16);
    s << "_unamed_v" << ++g_uv;
    return s;
}

inline fastring unamed_struct() {
    fastring s(16);
    s << "_unamed_s" << ++g_us;
    return s;
}

void decode_array(fs::fstream& fs, Array* a, const fastring& name, const fastring& js, int n) {
    Type* et = a->element_type();
    fastring ua = unamed_var();
    fastring uo = unamed_var();
    fs << indent(n) << "auto& " << ua << " = " << js << ";\n"
       << indent(n) << "for (uint32 i = 0; i < " << ua << ".array_size(); ++i) {\n";

    switch (et->type()) {
      case type_string:
        fs << indent(n + 4) << name << ".push_back(" << ua << "[i].as_c_str());\n";
        break;
      case type_bool:
        fs << indent(n + 4) << name << ".push_back(" << ua << "[i].as_bool());\n";
        break;
      case type_int:
      case type_int32:
      case type_int64:
      case type_uint32:
      case type_uint64:
        fs << indent(n + 4) << name << ".push_back((" << et->name() << ")" << ua << "[i].as_int64());\n";
        break;
      case type_double:
        fs << indent(n + 4) << name << ".push_back(" << ua << "[i].as_double());\n";
        break;
      case type_object:
        fs << indent(n + 4) << et->name() << " " << uo << ";\n";
        fs << indent(n + 4) << uo << ".from_json(" << ua << "[i]);\n";
        fs << indent(n + 4) << name << ".emplace_back(std::move(" << uo << "));\n";
        break;
      case type_array:
        fs << indent(n + 4) << "god::rm_ref_t<decltype(" << name << "[0])> " << uo << ";\n";
        decode_array(fs, (Array*)et, uo, ua + "[i]", n + 4);
        fs << indent(n + 4) << name << ".emplace_back(std::move(" << uo << "));\n";
        break;
      default:
        break;
    }

    fs << indent(n) << "}\n";
}

void encode_array(fs::fstream& fs, Array* a, const fastring& name, const fastring& js, int n) {
    Type* et = a->element_type();
    fs << indent(n) << "for (size_t i = 0; i < " << name << ".size(); ++i) {\n";

    switch (et->type()) {
      case type_string:
      case type_bool:
      case type_int:
      case type_int32:
      case type_int64:
      case type_uint32:
      case type_uint64:
      case type_double:
        fs << indent(n + 4) << js << ".push_back(" << name << "[i]);\n";
        break;
      case type_object:
        fs << indent(n + 4) << js << ".push_back(" << name << "[i].as_json());\n";
        break;
      case type_array:
        {
            fastring ua = unamed_var();
            fastring ub = unamed_var();
            fs << indent(n + 4) << "const auto& " << ua << " = " << name << "[i];\n";
            fs << indent(n + 4) << "co::Json " << ub << ";\n";
            encode_array(fs, (Array*)et, ua, ub, n + 4);
            fs << indent(n + 4) << js << ".push_back(" << ub << ");\n";
        }
        break;
      default:
        break;
    }

    fs << indent(n) << "}\n";
}

void gen_object(fs::fstream& fs, Object* o, int n=0) {
    fs << indent(n) << "struct " << o->name() << " {\n";
    const auto& aos = o->anony_objects();
    for (auto& ao : aos) {
        ao->set_name(unamed_struct());
        gen_object(fs, ao, n + 4);
    }

    // fields
    const auto& fields = o->fields();
    for (auto& f : fields) {
        Type* t = f->type();
        if (t->type() == type_string) {
            fs << indent(n + 4) << g_sname << ' ' << f->name() << ";\n";
        } else if (t->type() != type_array) {
            fs << indent(n + 4) << t->name() << ' ' << f->name() << ";\n";
        } else {
            auto x = array_proto((Array*)t);
            fs << indent(n + 4) << x << ' ' << f->name() << ";\n";
        }
    }

    if (!fields.empty()) fs << '\n';

    // method from_json
    fs << indent(n + 4) << "void from_json(const co::Json& _x_) {\n";
    for (auto& f : fields) {
        Type* t = f->type();
        const fastring& name = f->name();
        fastring js(32);
        js << "_x_.get(\"" << name << "\")";

        switch (t->type()) {
          case type_string:
            fs << indent(n + 8) << name << " = " << js << ".as_c_str();\n";
            break;
          case type_bool:
            fs << indent(n + 8) << name << " = " << js << ".as_bool();\n";
            break;
          case type_int:
          case type_int32:
          case type_int64:
          case type_uint32:
          case type_uint64:
            fs << indent(n + 8) << name << " = (" << t->name() << ")" << js << ".as_int64();\n";
            break;
          case type_double:
            fs << indent(n + 8) << name << " = " << js << ".as_double();\n";
            break;
          case type_object:
            fs << indent(n + 8) << name << ".from_json(" << js << ");\n";
            break;
          case type_array:
            g_uv = 0;
            fs << indent(n + 8) << "do {\n";
            decode_array(fs, (Array*)t, f->name(), js, n + 12);
            fs << indent(n + 8) << "} while (0);\n";
            break;
          default:
            break;
        }
    }
    fs << indent(n + 4) << "}\n\n";

    // method as_json()
    fs << indent(n + 4) << "co::Json as_json() const {\n";
    fs << indent(n + 8) << "co::Json _x_;\n";
    for (auto& f : fields) {
        Type* t = f->type();
        const fastring& name = f->name();

        switch (t->type()) {
          case type_string:
          case type_bool:
          case type_int:
          case type_int32:
          case type_int64:
          case type_uint32:
          case type_uint64:
          case type_double:
            fs << indent(n + 8) << "_x_.add_member(\"" << name << "\", " << name << ");\n";
            break;
          case type_object:
            fs << indent(n + 8) << "_x_.add_member(\"" << name << "\", " << name << ".as_json());\n";
            break;
          case type_array:
            {
                g_uv = 0;
                fastring uname = unamed_var();
                fs << indent(n + 8) << "do {\n";
                fs << indent(n + 12) << "co::Json " << uname << ";\n";
                encode_array(fs, (Array*)t, f->name(), uname, n + 12);
                fs << indent(n + 12) << "_x_.add_member(\"" << name << "\", " << uname << ");\n";
                fs << indent(n + 8) << "} while (0);\n";
            }
            break;
          default:
            break;
        }
    }
    fs << indent(n + 8) << "return _x_;\n";
    fs << indent(n + 4) << "}\n";

    fs << indent(n) << "};\n\n";
}

void gen(Program* p) {
    auto s = p->service();
    const auto& objects = p->objects();
    if (!s && objects.empty()) {
        co::print("no service or object found in proto file");
    }

    fastring f(p->fbase() + ".h");
    fs::fstream fs(f.c_str(), 'w');
    if (!fs) {
        co::print("failed to open file: ", f);
        exit(0);
    }

    // includes
    fs << "// Autogenerated.\n"
        << "// DO NOT EDIT. All changes will be undone.\n"
        << "#pragma once\n\n";

    if (s) {
        fs   << "#include \"co/rpc.h\"\n\n";
    } else {
        fs   << "#include \"co/json.h\"\n\n";
    }

    // namespace
    const auto& pkgs = p->pkgs();
    for (auto& pkg : pkgs) {
        fs << "namespace " << pkg << " {\n";
    }
    if (!pkgs.empty()) fs << "\n";

    // service
    if (s) gen_service(fs, s->name(), s->methods());

    // objects
    for (auto& o : objects) {
        g_us = 0;
        gen_object(fs, o, 0);
    }

    for (auto& pkg : pkgs) {
        fs << "} // " << pkg << "\n";
    }

    fs.flush();
    co::print("generate ", f, " success");
}

bool parse(const char* path) {
    yyin = fopen(path, "r");
    if (!yyin) {
        co::print("failed to open proto file ", path);
        return false;
    }
    defer(fclose(yyin));

    yylineno = 1;
    if (yyparse() != 0) {
        co::print("parse proto file ", path, " failed");
        return false;
    }

    const char* b = strrchr(path, '/');
    if (b == 0) b = strrchr(path, '\\');
    b == 0 ? (b = path) : ++b;
    const char* e = strrchr(b, '.');
    if (e == b) {
        co::print("invalid proto file name: ", path);
        return false;
    }

    g_prog->set_fbase(fastring(b, e ? e - b : strlen(b)));
    g_prog->set_fname(fastring(b));
    return true;
}

int main(int argc, char** argv) {
    auto v = flag::parse(argc, argv);
    if (v.empty()) {
        cout << "usage:\n"
             << "\tgen xx.proto\n"
             << "\tgen a.proto b.proto\n";
        return 0;
    }

    g_prog = co::make<Program>();
    g_sname = FLG_std ? "std::string" : "fastring";
    g_aname = FLG_std ? "std::vector" : "co::vector";

    for (size_t i = 0; i < v.size(); ++i) {
        if (parse(v[i].c_str())) {
            gen(g_prog);
            g_prog->clear();
        }
    }

    co::del(g_prog);
    return 0;
}
