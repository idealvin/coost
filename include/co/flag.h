#pragma once

#include "fastring.h"
#include <stdint.h>
#include <vector>

// co/flag is a library similar to Google's gflags.
// A flag is in fact a global variable, and value can be passed to it
// from command line or from a config file.

namespace flag {

// Parse command line flags or config file specified by -config.
// Return non-flag elements.
std::vector<fastring> init(int argc, char** argv);

namespace xx {
typedef fastring string;
typedef int32_t int32;
typedef int64_t int64;
typedef uint32_t uint32;
typedef uint64_t uint64;

enum {
    TYPE_bool,
    TYPE_int32,
    TYPE_int64,
    TYPE_uint32,
    TYPE_uint64,
    TYPE_string,
    TYPE_double
};

struct FlagSaver {
    FlagSaver(const char* type_str, const char* name, const char* value,
              const char* help, const char* file, int line, int type, void* addr);
};
} // namespace xx
} // namespace flag

#define _DECLARE_FLAG(type, name) \
    namespace flag { \
    namespace zz { \
        using namespace ::flag::xx; \
        extern type FLG_##name; \
    } \
    } \
    using flag::zz::FLG_##name

// Declare a flag.
// DEC_string(s);  ->  extern fastring FLG_s;
#define DEC_bool(name)    _DECLARE_FLAG(bool, name)
#define DEC_int32(name)   _DECLARE_FLAG(int32, name)
#define DEC_int64(name)   _DECLARE_FLAG(int64, name)
#define DEC_uint32(name)  _DECLARE_FLAG(uint32, name)
#define DEC_uint64(name)  _DECLARE_FLAG(uint64, name)
#define DEC_string(name)  _DECLARE_FLAG(string, name)
#define DEC_double(name)  _DECLARE_FLAG(double, name)

#define _DEFINE_FLAG(type, name, value, help) \
    namespace flag { \
    namespace zz { \
        using namespace ::flag::xx; \
        type FLG_##name = value; \
        static ::flag::xx::FlagSaver _Sav_flag_##name( \
            #type, #name, #value, help, __FILE__, __LINE__, ::flag::xx::TYPE_##type, &FLG_##name \
        ); \
    } \
    } \
    using flag::zz::FLG_##name

// Define a flag.
// DEF_int32(i, 23, "xxx");  ->  int32 FLG_i = 23
#define DEF_bool(name, value, help)    _DEFINE_FLAG(bool, name, value, help)
#define DEF_int32(name, value, help)   _DEFINE_FLAG(int32, name, value, help)
#define DEF_int64(name, value, help)   _DEFINE_FLAG(int64, name, value, help)
#define DEF_uint32(name, value, help)  _DEFINE_FLAG(uint32, name, value, help)
#define DEF_uint64(name, value, help)  _DEFINE_FLAG(uint64, name, value, help)
#define DEF_string(name, value, help)  _DEFINE_FLAG(string, name, value, help)
#define DEF_double(name, value, help)  _DEFINE_FLAG(double, name, value, help)

DEC_string(help);
DEC_string(config);
