#include "co/unitest.h"
#include "co/time.h"

namespace unitest {
namespace xx {

static co::vector<Test>* g_t;

inline co::vector<Test>& tests() {
    return g_t ? *g_t : *(g_t = co::_make_static<co::vector<Test>>());
}

bool add_test(const char* name, bool& e, void(*f)(Test&)) {
    tests().push_back(Test(name, e, f));
    return true;
}

} // xx

int run_tests() {
    // n: number of tests to do
    // ft: number of failed tests
    // fc: number of failed cases
    int n = 0, ft = 0, fc = 0;
    co::Timer timer;
    auto& tests = xx::tests();

    co::vector<xx::Test*> enabled(32);
    for (auto& t: tests) if (t.enabled) enabled.push_back(&t);

    if (enabled.empty()) { /* run all tests by default */
        n = tests.size();
        for (auto& t : tests) {
            cout << "> begin test: " << t.name << endl;
            timer.restart();
            t.f(t);
            if (!t.failed.empty()) { ++ft; fc += t.failed.size(); }
            cout << "< test " << t.name << " done in " << timer.us() << " us" << endl;
        }

    } else {
        n = enabled.size();
        for (auto& t: enabled) {
            cout << "> begin test: " << t->name << endl;
            timer.restart();
            t->f(*t);
            if (!t->failed.empty()) { ++ft; fc += t->failed.size(); }
            cout << "< test " << t->name << " done in " << timer.us() << " us" << endl;
        }
    }

    if (fc == 0) {
        if (n > 0) {
            cout << color::green << "\nCongratulations! All tests passed!" << color::deflt << endl;
        } else {
            cout << "No test found. Done nothing." << endl;
        }

    } else {
        cout << color::red << "\nAha! " << fc << " case" << (fc > 1 ? "s" : "");
        cout << " from " << ft << " test" << (ft > 1 ? "s" : "");
        cout << " failed. See details below:\n" << color::deflt << endl;

        const char* last_case = "";
        for (auto& t : tests) {
            if (!t.failed.empty()) {
                cout << color::red << "In test " << t.name << ":\n" << color::deflt;
                for (auto& f : t.failed) {
                    if (strcmp(last_case, f.c) != 0) {
                        last_case = f.c;
                        cout << color::red << " case " << f.c << ":\n" << color::deflt;
                    }
                    cout << color::yellow << "  " << f.file << ':' << f.line << "] "
                        << color::deflt << f.msg << '\n';
                }
                cout.flush();
            }
        }

        cout << color::deflt;
        cout.flush();
    }

    return fc;
}

} // namespace unitest
