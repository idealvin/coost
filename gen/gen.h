#pragma once

#include "co/fastring.h"
#include "co/stl.h"
#include "co/cout.h"

extern "C" {
int yylex(void);
int yyparse(void);
void yyerror(const char* s);
}

extern int yylineno;
extern char yytext[];
extern FILE* yyin;

// @s is the result of co::strdup()
inline fastring S(char* s) {
    const size_t n = strlen(s);
    return fastring(s, n + 1, n);
}

class Service {
  public:
    Service() = default;
    ~Service() = default;

    const fastring& name() const { return _name; }
    void set_name(fastring&& x) { _name = std::move(x); }

    bool add_method(const fastring& m) {
        auto it = _keys.insert(m);
        if (!it.second) return false;
        _methods.push_back(m);
        return true;
    }

    const co::vector<fastring>& methods() const { return _methods; }

  private:
    fastring _name;
    co::vector<fastring> _methods;
    co::hash_set<fastring> _keys; 
};

enum type_t {
    type_int,
    type_int32,
    type_int64,
    type_uint32,
    type_uint64,
    type_bool,
    type_double,
    type_string,
    type_array,
    type_object,
};

class Type {
  public:
    Type() = default;
    virtual ~Type() = default;

    const fastring& name() const { return _name; }
    void set_name(fastring&& x) { _name = std::move(x); }
    type_t type() const { return _type; }
    void set_type(type_t x) { _type = x; }

  protected:
    fastring _name;
    type_t _type;
};

class Value {
  public:
    Value() = default;
    ~Value() {
        if (_type == type_string && _s) {
            co::free(_s, strlen(_s) + 1);
        }
    }

    type_t type() const { return _type; }

    bool get_bool() const { return _b; }
    int64 get_integer() const { return _i; }
    double get_double() const { return _d; }
    char* get_string() const { return _s; }

    void set_bool(bool x) { _type = type_bool; _b = x; }
    void set_integer(int64 x) { _type = type_int64; _i = x; }
    void set_double(double x) { _type = type_double; _d = x; }
    void set_string(char* s) { _type = type_string; _s = s; }

  private:
    type_t _type;
    union {
        bool _b;
        int64 _i;
        double _d;
        char* _s;
    };
};

// anonymous 
class Field {
  public:
    Field() : _type(0), _value(0) {}
    ~Field() {
        if (_type->type() != type_object) co::del(_type);
        if (_value) co::del(_value);
    }

    const fastring& name() const { return _name; }
    void set_name(fastring&& x) { _name = std::move(x); }

    Type* type() const { return _type; }
    void set_type(Type* x) { _type = x; }

    Value* value() const { return _value; }
    void set_value(Value* v) { _value = v; }

  private:
    fastring _name;
    Type* _type;
    Value* _value;
};

class Array : public Type {
  public:
    Array() {
        _type = type_array;
    }

    virtual ~Array() {
        if (_element_type->type() != type_object) co::del(_element_type);
    }

    Type* element_type() const {
        return _element_type;
    }

    void set_element_type(Type* t) {
        _element_type = t;
    }

  private:
    Type* _element_type;
};

class Object : public Type {
  public:
    Object() {
        _type = type_object;
    }

    virtual ~Object() {
        for (auto& x : _fields) co::del(x);
        for (auto& x : _anony_objects) co::del(x);
    }

    bool add_field(Field* f) {
        auto it = _keys.insert(f->name());
        if (!it.second) return false;
        _fields.push_back(f);
        return true;
    }

    const co::vector<Field*>& fields() const {
        return _fields;
    }

    const co::vector<Object*>& anony_objects() const {
        return _anony_objects;
    }

    void set_anony_objects(co::vector<Object*>&& x) {
        _anony_objects = std::move(x);
    }

  private:
    co::vector<Field*> _fields;
    co::hash_set<fastring> _keys;
    co::vector<Object*> _anony_objects;
};

class Program {
  public:
    Program() : _serv(0) {}
    ~Program() {
        this->clear();
    }

    void set_fbase(fastring&& x) { _fbase = std::move(x); }
    const fastring& fbase() const { return _fbase; }

    void set_fname(fastring&& x) { _fname = std::move(x); }
    const fastring& fname() const { return _fname; }

    void add_pkg(fastring&& x) {
        _pkgs.emplace_back(std::move(x));
    }

    const co::vector<fastring>& pkgs() const { return _pkgs; }

    void set_service(Service* s) { _serv = s; }
    Service* service() const { return _serv; }

    bool add_object(Object* x) {
        auto it = _idx.emplace(x->name(), _objects.size());
        if (!it.second) return false;
        _objects.push_back(x);
        x->set_anony_objects(std::move(_anony_objects));
        return true;
    }

    void add_anony_object(Object* x) {
        _anony_objects.push_back(x);
    }

    Object* find_object(const fastring& name) const {
        auto it = _idx.find(name);
        if (it != _idx.end()) return _objects[it->second];
        return nullptr;
    }

    const co::vector<Object*>& objects() const {
        return _objects;
    }

    void clear() {
        _fbase.clear();
        _fname.clear();
        _pkgs.clear();
        co::del(_serv);
        _serv = 0;
        for (auto& x : _objects) co::del(x);
        _objects.clear();
        _idx.clear();
    }

  private:
    fastring _fbase; // base name of proto file
    fastring _fname; // name of the proto file
    co::vector<fastring> _pkgs;
    Service* _serv;
    co::vector<Object*> _objects;
    co::vector<Object*> _anony_objects;
    co::hash_map<fastring, size_t> _idx;
};

extern Program* g_prog;
