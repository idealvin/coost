#pragma once

#include "flag.h"
#include "cout.h"
#include "fastring.h"

// co/unitest is an unit test framework similar to google's gtests.

namespace unitest {

__coapi int run_all_tests();

__coapi void push_failed_msg(
    const fastring& test_name, const fastring& case_name, 
    const char* file, int line, const fastring& msg
);

struct Test {
    Test() = default;
    virtual ~Test() = default;

    virtual void run() = 0;
    virtual bool enabled() = 0;
    virtual const fastring& name() = 0;

    DISALLOW_COPY_AND_ASSIGN(Test);
};

struct __coapi TestSaver {
    TestSaver(Test* test);
};

struct Case {
    Case(const fastring& name)
        : _name(name) {
        cout << " case " << _name << ':' << endl;
    }

    const fastring& name() const {
        return _name;
    }

  private:
    fastring _name;
};

} // namespace unitest

// define a test unit
#define DEF_test(_name_) \
    DEF_bool(_name_, false, "enable this test if true"); \
    \
    struct _UTest_##_name_ : public unitest::Test { \
        _UTest_##_name_() : _name(#_name_) {} \
        virtual ~_UTest_##_name_() {} \
        \
        virtual void run(); \
        virtual bool enabled() { return FLG_##_name_; } \
        virtual const fastring& name() { return _name; } \
        \
      private: \
        fastring _name; \
        co::unique_ptr<unitest::Case> _current_case; \
    }; \
    \
    static unitest::TestSaver _UT_sav_test_##_name_(co::make<_UTest_##_name_>()); \
    \
    void _UTest_##_name_::run()

// define a test case in the current unit
#define DEF_case(name) _current_case.reset(co::make<unitest::Case>(#name));

#define EXPECT(x) \
{ \
    if (_current_case == NULL) { \
        _current_case.reset(co::make<unitest::Case>("default")); \
    } \
    \
    if (x) { \
        cout << color::green << "  EXPECT(" << #x << ") passed" \
             << color::deflt << endl; \
    } else { \
        fastring _U_s(32); \
        _U_s << "EXPECT(" << #x << ") failed"; \
        cout << color::red << "  " << _U_s << color::deflt << endl; \
        unitest::push_failed_msg(_name, _current_case->name(), __FILE__, __LINE__, _U_s); \
    } \
}

#define EXPECT_OP(x, y, op, opname) \
{ \
    if (_current_case == NULL) { \
        _current_case.reset(co::make<unitest::Case>("default")); \
    } \
    \
    auto _U_x = (x); \
    auto _U_y = (y); \
    \
    if (_U_x op _U_y) { \
        cout << color::green \
             << "  EXPECT_" << opname << "(" << #x << ", " << #y << ") passed"; \
        if (strcmp("==", #op) != 0) cout << ": " << _U_x << " vs " << _U_y; \
        cout << color::deflt << endl; \
        \
    } else { \
        fastring _U_s(128); \
        _U_s << "EXPECT_" << opname << "(" << #x << ", " << #y << ") failed: " \
             << _U_x << " vs " << _U_y; \
        cout << color::red << "  " << _U_s << color::deflt << endl; \
        unitest::push_failed_msg(_name, _current_case->name(), __FILE__, __LINE__, _U_s); \
    } \
}

#define EXPECT_EQ(x, y) EXPECT_OP(x, y, ==, "EQ")
#define EXPECT_NE(x, y) EXPECT_OP(x, y, !=, "NE")
#define EXPECT_GE(x, y) EXPECT_OP(x, y, >=, "GE")
#define EXPECT_LE(x, y) EXPECT_OP(x, y, <=, "LE")
#define EXPECT_GT(x, y) EXPECT_OP(x, y, >, "GT")
#define EXPECT_LT(x, y) EXPECT_OP(x, y, <, "LT")
