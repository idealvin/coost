#include "co/unitest.h"
#include "co/time.h"
#include "co/os.h"

#include <vector>
#include <map>

#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning(disable:4503)
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

DEF_bool(a, false, ">>.Run all tests if true");

namespace unitest {
using std::cout;
using std::endl;

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
    std::vector<Test*>& tests = gTests();
    int n = 0;
    Timer t;

    for (size_t i = 0; i < tests.size(); ++i) {
        Test* test = tests[i];

        if (FLG_a || test->enabled()) {
            cout << "> begin test: " << test->name() << endl;
            t.restart();
            test->run();
            cout << "< test " << test->name() << " done in " << t.us() << " us" << endl;
            ++n;
        }

        delete test;
    }

    TMap& failed = failed_tests();
    if (failed.empty()) {
        if (n > 0) {
            cout << green << "\nCongratulations! All tests passed!" << default_color << endl;
        } else {
            cout << "Done nothing. Try running with -a!" << endl;
        }

    } else {
        size_t ntestsfailed = failed.size();
        size_t ncasesfailed = 0;

        for (TMap::iterator it  = failed.begin(); it != failed.end(); ++it) {
            CMap& cases = it->second;
            ncasesfailed += cases.size();
        }

        cout << red << "\nAha! " << ncasesfailed << " case" << (ncasesfailed > 1 ? "s" : "");
        cout << " from " << ntestsfailed << " test" << (ncasesfailed > 1 ? "s" : "");
        cout << " failed. See details below:\n" << default_color << endl;
        
        for (auto it = failed.begin(); it != failed.end(); ++it) {
            cout << red << "In test " << it->first << ":\n" << default_color;

            CMap& cases = it->second;
            for (auto ct = cases.begin(); ct != cases.end(); ++ct) {
                cout << red << " case " << ct->first << ":\n" << default_color;

                std::vector<FailedMsg*>& msgs = ct->second;
                for (size_t i = 0; i < msgs.size(); ++i) {
                    FailedMsg* msg = msgs[i];
                    cout << yellow << "  " << msg->file << ':' << msg->line << "] "
                         << default_color << msg->msg << '\n';
                    delete msg;
                }
            }

            cout.flush();
        }

        cout << default_color;
        cout.flush();
    }
}

#ifdef _WIN32
inline bool ansi_color_seq_enabled() {
    static const bool x = !os::env("TERM").empty();
    return x;
}

inline HANDLE get_std_handle() {
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE); 
    if (handle != INVALID_HANDLE_VALUE && handle != NULL) return handle;
    return NULL;
}

inline HANDLE& std_handle() {
    static HANDLE handle = get_std_handle();
    return handle;
}

inline int get_default_color() {
    HANDLE handle = std_handle();
    if (!handle) return 15;

    CONSOLE_SCREEN_BUFFER_INFO buf;
    if (!GetConsoleScreenBufferInfo(handle, &buf)) return 15;
    return buf.wAttributes & 0x0f;
}

Color::Color(const char* ansi_seq, int win_color) {
    ansi_color_seq_enabled() ? (void)(s = ansi_seq) : (void)(i = win_color);
}

const Color red("\033[38;5;1m", FOREGROUND_RED);     // 12
const Color green("\033[38;5;2m", FOREGROUND_GREEN); // 10
const Color blue("\033[38;5;12m", FOREGROUND_BLUE);  // 9
const Color yellow("\033[38;5;11m", 14);
const Color default_color("\033[39m", get_default_color());

std::ostream& operator<<(std::ostream&os, const Color& color) {
    if (ansi_color_seq_enabled()) {
        os << color.s;
        return os;
    } else {
        if (std_handle()) SetConsoleTextAttribute(std_handle(), (WORD)color.i);
        return os;
    }
}
#endif

} // namespace unitest
