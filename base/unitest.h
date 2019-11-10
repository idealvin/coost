#pragma once

#include "flag.h"
#include "fastream.h"

#include <memory>
#include <iostream>

// UNITEST is a C++ unit testing framework similar to google's gtests.

namespace unitest {

void init(int argc, char** argv);

void run_all_tests();

void push_failed_msg(const fastring& test_name, const fastring& case_name,
                     const char* file, int line, const fastring& msg);

class Test {
  public:
    Test() {}
    virtual ~Test() {}

    virtual void run() = 0;
    virtual bool enabled() = 0;
    virtual const fastring& name() = 0;

  private:
    Test(const Test&);
    void operator=(const Test&);
};

struct TestSaver {
    TestSaver(Test* test);
};

class Case {
  public:
    Case(const fastring& name)
        : _name(name) {
        std::cout << " case " << _name << ':' << std::endl;
    }

    const fastring& name() const {
        return _name;
    }

  private:
    fastring _name;
};

#ifdef _WIN32
struct Color {
    Color(const char* s, int i);

    union {
        int i;
        const char* s;
    };
};

std::ostream& operator<<(std::ostream&, const Color&);

extern const Color red;
extern const Color green;
extern const Color blue;
extern const Color yellow;
extern const Color default_color;

#else
struct Color {};
inline void operator<<(std::ostream&, const Color&) {}

const char* const red = "\033[38;5;1m";
const char* const green = "\033[38;5;2m";
const char* const blue = "\033[38;5;12m";
const char* const yellow = "\033[38;5;3m"; // or 11
const char* const default_color = "\033[39m";
#endif

} // namespace unitest

#define DEF_test(_name_) \
    DEF_bool(_name_, false, "enable this test if true."); \
    using std::cout; \
    using std::endl; \
    using unitest::operator<<; \
    \
    struct _UTest_##_name_ : public unitest::Test { \
        _UTest_##_name_() : _name(#_name_) {} \
        \
        virtual ~_UTest_##_name_() {} \
        \
        virtual void run(); \
        \
        virtual bool enabled() { \
            return FLG_##_name_; \
        } \
        \
        virtual const fastring& name() { \
            return _name; \
        } \
        \
      private: \
        fastring _name; \
        std::unique_ptr<unitest::Case> _current_case; \
    }; \
    \
    static unitest::TestSaver _UT_Sav_test_##_name_(new _UTest_##_name_()); \
    \
    void _UTest_##_name_::run()

#define DEF_case(name) _current_case.reset(new unitest::Case(#name));

#define EXPECT(x) \
{ \
    if (_current_case == NULL) { \
        _current_case.reset(new unitest::Case("default")); \
    } \
    \
    if (x) { \
        cout << unitest::green << "  EXPECT(" << #x << ") passed" \
             << unitest::default_color << endl; \
        \
    } else { \
        magicstream _U_fs; \
        _U_fs << "EXPECT(" << #x << ") failed"; \
        fastring _U_s = _U_fs.str(); \
        \
        cout << unitest::red << "  " << _U_s << unitest::default_color << endl; \
        unitest::push_failed_msg(_name, _current_case->name(), __FILE__, __LINE__, _U_s); \
    } \
}

#define EXPECT_OP(x, y, op, opname) \
{ \
    if (_current_case == NULL) { \
        _current_case.reset(new unitest::Case("default")); \
    } \
    \
    auto _U_x = x; \
    auto _U_y = y; \
    \
    if (_U_x op _U_y) { \
        cout << unitest::green \
             << "  EXPECT_" << opname << "(" << #x << ", " << #y << ") passed"; \
        if (strcmp("==", #op) != 0) cout << ": " << _U_x << " vs " << _U_y; \
        cout << unitest::default_color << endl; \
        \
    } else { \
        magicstream _U_fs; \
        _U_fs << "EXPECT_" << opname << "(" << #x << ", " << #y << ") failed: " \
              << _U_x << " vs " << _U_y; \
        fastring _U_s = _U_fs.str(); \
        \
        cout << unitest::red << "  " << _U_s << unitest::default_color << endl; \
        unitest::push_failed_msg(_name, _current_case->name(), __FILE__, __LINE__, _U_s); \
    } \
}

#define EXPECT_EQ(x, y) EXPECT_OP(x, y, ==, "EQ")
#define EXPECT_NE(x, y) EXPECT_OP(x, y, !=, "NE")
#define EXPECT_GE(x, y) EXPECT_OP(x, y, >=, "GE")
#define EXPECT_LE(x, y) EXPECT_OP(x, y, <=, "LE")
#define EXPECT_GT(x, y) EXPECT_OP(x, y, >, "GT")
#define EXPECT_LT(x, y) EXPECT_OP(x, y, <, "LT")
