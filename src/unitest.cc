#include "co/unitest.h"
#include "co/time.h"
#include <vector>
#include <map>

namespace unitest {

inline std::vector<Test*>& gTests() {
    static std::vector<Test*> tests;
    return tests;
}

TestSaver::TestSaver(Test* test) {
    gTests().push_back(test);
}

struct FailedMsg {
    FailedMsg(const char* f, int n, const fastring& s)
        : file(f), line(n), msg(s) {
    }
    
    const char* file;
    int line;
    fastring msg;
};

typedef std::map<fastring, std::vector<FailedMsg*>> CMap; // <case_name, msgs>
typedef std::map<fastring, CMap> TMap;                    // <test_name, cases>

inline TMap& failed_tests() {
    static TMap m;
    return m;
}

void push_failed_msg(const fastring& test_name, const fastring& case_name,
                     const char* file, int line, const fastring& msg) {
    TMap& x = failed_tests();
    x[test_name][case_name].push_back(new FailedMsg(file, line, msg));
}

void run_all_tests() {
    Timer t;
    int n = 0;
    auto& tests = gTests();

    std::vector<Test*> enabled;
    for (auto& test : tests) {
        if (test->enabled()) enabled.push_back(test);
    }

    // enable all by default
    if (enabled.empty()) { /* run all tests by default */
        n = tests.size();
        for (auto& test : tests) {
            cout << "> begin test: " << test->name() << endl;
            t.restart();
            test->run();
            cout << "< test " << test->name() << " done in " << t.us() << " us" << endl;
            delete test;
        }

    } else {
        n = enabled.size();
        for (auto& test: enabled) {
            cout << "> begin test: " << test->name() << endl;
            t.restart();
            test->run();
            cout << "< test " << test->name() << " done in " << t.us() << " us" << endl;
        }

        for (auto& test: tests) delete test;
    }

    TMap& failed = failed_tests();
    if (failed.empty()) {
        if (n > 0) {
            cout << color::green << "\nCongratulations! All tests passed!" << color::deflt << endl;
        } else {
            cout << "No test found. Done nothing." << endl;
        }

    } else {
        size_t ntestsfailed = failed.size();
        size_t ncasesfailed = 0;

        for (TMap::iterator it  = failed.begin(); it != failed.end(); ++it) {
            CMap& cases = it->second;
            ncasesfailed += cases.size();
        }

        cout << color::red << "\nAha! " << ncasesfailed << " case" << (ncasesfailed > 1 ? "s" : "");
        cout << " from " << ntestsfailed << " test" << (ncasesfailed > 1 ? "s" : "");
        cout << " failed. See details below:\n" << color::deflt << endl;
        
        for (auto it = failed.begin(); it != failed.end(); ++it) {
            cout << color::red << "In test " << it->first << ":\n" << color::deflt;

            CMap& cases = it->second;
            for (auto ct = cases.begin(); ct != cases.end(); ++ct) {
                cout << color::red << " case " << ct->first << ":\n" << color::deflt;

                std::vector<FailedMsg*>& msgs = ct->second;
                for (size_t i = 0; i < msgs.size(); ++i) {
                    FailedMsg* msg = msgs[i];
                    cout << color::yellow << "  " << msg->file << ':' << msg->line << "] "
                         << color::deflt << msg->msg << '\n';
                    delete msg;
                }
            }

            cout.flush();
        }

        cout << color::deflt;
        cout.flush();
    }
}

} // namespace unitest
