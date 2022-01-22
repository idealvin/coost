#pragma once

#include "def.h"
#include "fastring.h"
#include <vector>

// co/flag is a library similar to Google's gflags.
// A flag is in fact a global variable, and value can be passed to it
// from command line or from a config file.

namespace flag {

// Parse command line flags or config file specified by -config.
// Return non-flag elements.
__coapi std::vector<fastring> init(int argc, const char** argv);

inline std::vector<fastring> init(int argc, char** argv) {
    return flag::init(argc, (const char**)argv);
}

// Initialize with a config file.
__coapi void init(const fastring& path);

// Set value of a flag, return error message if failed.
__coapi fastring set_value(const fastring& name, const fastring& value);

namespace xx {

__coapi void add_flag(
    char type, const char* name, const char* value, const char* help, 
    const char* file, int line, void* addr
);

} // namespace xx
} // namespace flag

#define _CO_DEC_FLAG(type, name) extern type FLG_##name

// Declare a flag.
// DEC_string(s);  ->  extern fastring FLG_s;
#define DEC_bool(name)    _CO_DEC_FLAG(bool, name)
#define DEC_int32(name)   _CO_DEC_FLAG(int32, name)
#define DEC_int64(name)   _CO_DEC_FLAG(int64, name)
#define DEC_uint32(name)  _CO_DEC_FLAG(uint32, name)
#define DEC_uint64(name)  _CO_DEC_FLAG(uint64, name)
#define DEC_double(name)  _CO_DEC_FLAG(double, name)
#define DEC_string(name)  extern fastring FLG_##name

#define _CO_DEF_FLAG(type, id, name, value, help) \
    type FLG_##name = []() { \
        ::flag::xx::add_flag(id, #name, #value, help, __FILE__, __LINE__, &FLG_##name); \
        return value; \
    }()

// Define a flag.
// DEF_int32(i, 23, "xxx");  ->  int32 FLG_i = 23
#define DEF_bool(name, value, help)    _CO_DEF_FLAG(bool,   'b', name, value, help)
#define DEF_int32(name, value, help)   _CO_DEF_FLAG(int32,  'i', name, value, help)
#define DEF_int64(name, value, help)   _CO_DEF_FLAG(int64,  'I', name, value, help)
#define DEF_uint32(name, value, help)  _CO_DEF_FLAG(uint32, 'u', name, value, help)
#define DEF_uint64(name, value, help)  _CO_DEF_FLAG(uint64, 'U', name, value, help)
#define DEF_double(name, value, help)  _CO_DEF_FLAG(double, 'd', name, value, help)

#define DEF_string(name, value, help) \
    fastring FLG_##name = []() { \
        ::flag::xx::add_flag('s', #name, #value, help, __FILE__, __LINE__, &FLG_##name); \
        return value; \
    }()

__coapi DEC_string(help);
__coapi DEC_string(config);
__coapi DEC_string(version);
