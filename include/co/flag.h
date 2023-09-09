#pragma once

#include "def.h"
#include "mem.h"
#include "vector.h"
#include "fastring.h"

namespace flag {

// parse command line arguments.
// config file specified by -conf or -config will also be parsed.
__coapi co::vector<fastring> parse(int argc, char** argv);

// parse config file
__coapi void parse(const fastring& path);

// deprecated, use flag::parse() instead
inline co::vector<fastring> init(int argc, char** argv) {
    return flag::parse(argc, argv);
}

// set value for a flag
//   - It is not thread-safe. 
//   - eg.
//     flag::set_value("co_sched_num", "1");
//     flag::set_value("cout", "true");
__coapi fastring set_value(const char* name, const fastring& value);

// add alias for a flag
//   - It is not thread-safe. 
//   - It should be used before calling flag::parse(argc, argv).
//   - @new_name should be a literal string, or a value with static life cycle.
__coapi fastring alias(const char* name, const char* new_name);

namespace xx {
__coapi void add_flag(
    char type, const char* name, const char* value, const char* help, 
    const char* file, int line, void* addr, const char* alias
);
} // xx
} // flag

#define _CO_DEC_FLAG(type, name) extern type FLG_##name

// declare a flag
// DEC_bool(b);    ->  extern bool FLG_b;
// DEC_string(s);  ->  extern fastring& FLG_s;
#define DEC_bool(name)    _CO_DEC_FLAG(bool, name)
#define DEC_int32(name)   _CO_DEC_FLAG(int32, name)
#define DEC_int64(name)   _CO_DEC_FLAG(int64, name)
#define DEC_uint32(name)  _CO_DEC_FLAG(uint32, name)
#define DEC_uint64(name)  _CO_DEC_FLAG(uint64, name)
#define DEC_double(name)  _CO_DEC_FLAG(double, name)
#define DEC_string(name)  extern fastring& FLG_##name

#define _CO_DEF_FLAG(type, iden, name, value, help, ...) \
    type FLG_##name = []() { \
        ::flag::xx::add_flag(iden, #name, #value, help, __FILE__, __LINE__, &FLG_##name, ""#__VA_ARGS__); \
        return value; \
    }()

// define a flag
// DEF_int32(i, 23, "xxx");         ->  int32 FLG_i = 23;
// DEF_bool(debug, false, "x", d);  ->  define a flag with an alias
#define DEF_bool(name, value, help, ...)    _CO_DEF_FLAG(bool,   'b', name, value, help, __VA_ARGS__)
#define DEF_int32(name, value, help, ...)   _CO_DEF_FLAG(int32,  'i', name, value, help, __VA_ARGS__)
#define DEF_int64(name, value, help, ...)   _CO_DEF_FLAG(int64,  'I', name, value, help, __VA_ARGS__)
#define DEF_uint32(name, value, help, ...)  _CO_DEF_FLAG(uint32, 'u', name, value, help, __VA_ARGS__)
#define DEF_uint64(name, value, help, ...)  _CO_DEF_FLAG(uint64, 'U', name, value, help, __VA_ARGS__)
#define DEF_double(name, value, help, ...)  _CO_DEF_FLAG(double, 'd', name, value, help, __VA_ARGS__)

#define DEF_string(name, value, help, ...) \
    fastring& FLG_##name = *[]() { \
        auto _##name = ::co::_make_static<fastring>(value); \
        ::flag::xx::add_flag('s', #name, #value, help, __FILE__, __LINE__, _##name, ""#__VA_ARGS__); \
        return _##name; \
    }()

// export FLG_help, FLG_config, FLG_version
__coapi DEC_string(help);
__coapi DEC_string(config);
__coapi DEC_string(version);
