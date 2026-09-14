#pragma once

#include "print.h"
#include "flag.h"

namespace co {

// return number of failed test cases
int run_unitests();

namespace ut {

struct Failed {
    Failed(const char* c, const char* file, int line, co::string&& msg)
        : c(c), file(file), line(line), msg(std::move(msg)) {
    }
    const char* c; // case name
    const char* file;
    int line;
    co::string msg;
};

struct Test {
    Test(const char* name, bool& e, void (*f)(Test&)) noexcept
        : name(name), c("default"), enabled(e), f(f) {
    }
    const char* name; // test name
    const char* c;    // current case name
    bool& enabled;    // if this test unit is enabled
    void (*f)(Test&);
    co::vector<Failed> failed;
};

bool add_test(const char* name, bool& e, void(*f)(Test&));

} // ut
} // co

// define a test unit
#define DEF_test(name) \
    DEF_bool(name, false, "enable this test if true"); \
    void _co_ut_##name(co::ut::Test&); \
    static bool _co_ut_v_##name = co::ut::add_test(#name, FLG_##name, _co_ut_##name); \
    void _co_ut_##name(co::ut::Test& _t_)

// define a test case in the current unit
#define DEF_case(name) _t_.c = #name; co::print(" case ", #name, ':', '\n');

#define EXPECT(x) \
do { \
    if (x) { \
        co::print(co::color::green, "  EXPECT(", #x, ") passed", co::color::deflt, '\n'); \
    } else { \
        co::string _s_(32); \
        _s_.cat("EXPECT(", #x, ") failed"); \
        co::print(co::color::red, "  ", _s_, co::color::deflt, '\n'); \
        _t_.failed.push_back(co::ut::Failed(_t_.c, __FILE__, __LINE__, std::move(_s_))); \
    } \
} while (0);

#define EXPECT_OP(x, y, op, opname) \
do { \
    auto _V_x = (x); \
    auto _V_y = (y); \
    if (_V_x op _V_y) { \
        co::print(co::color::green, "  EXPECT_", opname, '(', #x, ", ", #y, ") passed"); \
        if (strcmp(#op, "==") != 0) co::print(": ", _V_x, " vs ", _V_y); \
        co::print(co::color::deflt, '\n'); \
    } else { \
        co::string _s_(128); \
        _s_.cat("EXPECT_", opname, '(', #x, ", ", #y, ") failed: ", _V_x, " vs ", _V_y); \
        co::print(co::color::red, "  ", _s_, co::color::deflt, '\n'); \
        _t_.failed.push_back(co::ut::Failed(_t_.c, __FILE__, __LINE__, std::move(_s_))); \
    } \
} while (0);

#define EXPECT_EQ(x, y) EXPECT_OP(x, y, ==, "EQ")
#define EXPECT_NE(x, y) EXPECT_OP(x, y, !=, "NE")
#define EXPECT_GE(x, y) EXPECT_OP(x, y, >=, "GE")
#define EXPECT_LE(x, y) EXPECT_OP(x, y, <=, "LE")
#define EXPECT_GT(x, y) EXPECT_OP(x, y, >, "GT")
#define EXPECT_LT(x, y) EXPECT_OP(x, y, <, "LT")
