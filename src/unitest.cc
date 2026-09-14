#include "co/unitest.h"
#include "co/time.h"

namespace co {
namespace ut {

static co::vector<Test>* g_t;
inline co::vector<Test>& tests() {
    return g_t ? *g_t : *(g_t = co::_make_static<co::vector<Test>>());
}

bool add_test(const char* name, bool& e, void(*f)(Test&)) {
    tests().emplace_back(name, e, f);
    return false;
}

} // ut

int run_unitests() {
    // n: number of tests to do
    // ft: number of failed tests
    // fc: number of failed cases
    size_t n = 0, ft = 0, fc = 0;
    time::timer timer;
    auto& tests = ut::tests();

    co::vector<ut::Test*> enabled;
    for (auto& t : tests) if (t.enabled) enabled.push_back(&t);

    if (enabled.empty()) { /* run all tests by default */
        n = tests.size();
        for (auto& t : tests) {
            co::print("> begin test: ", t.name, '\n');
            timer.restart();
            t.f(t);
            if (!t.failed.empty()) { ++ft; fc += t.failed.size(); }
            co::print("< test ", t.name, " done in ", timer.us(), " us", '\n').flush();
        }

    } else {
        n = enabled.size();
        for (auto& t: enabled) {
            co::print("> begin test: ", t->name, '\n');
            timer.restart();
            t->f(*t);
            if (!t->failed.empty()) { ++ft; fc += t->failed.size(); }
            co::print("< test ", t->name, " done in ", timer.us(), " us", '\n').flush();
        }
    }

    if (fc == 0) {
        if (n > 0) {
            co::print(color::green, "\nNice! All tests passed!", color::deflt, '\n');
        } else {
            co::print("No test found. Done nothing.", '\n');
        }

    } else {
        co::print(color::red,
            "\nAha! ", fc, " case", (fc > 1 ? "s" : ""),
            " from ", ft, " test", (ft > 1 ? "s" : ""),
            " failed:\n\n", color::deflt
        );

        const char* last_case = "";
        for (auto& t : tests) {
            if (!t.failed.empty()) {
                co::print(color::red, "In test ", t.name, ":\n", color::deflt);
                for (auto& f : t.failed) {
                    if (strcmp(last_case, f.c) != 0) {
                        last_case = f.c;
                        co::print(color::red, " case ", f.c, ":\n", color::deflt);
                    }
                    co::print(color::yellow, "  ", f.file, ':', f.line, "] ", color::deflt, f.msg, '\n');
                }
                co::print('\n');
            }
        }

        co::print(color::deflt);
        co::print('\n').flush();
    }

    return (int)fc;
}

} // co
