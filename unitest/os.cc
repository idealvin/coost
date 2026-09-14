#include "co/unitest.h"
#include "co/os.h"

namespace test {

DEF_test(os) {
    DEF_case(env) {
        EXPECT_EQ(os::env("CO_TEST"), co::string());
        os::env("CO_TEST", "777");
        auto s = os::env("CO_TEST");
        EXPECT_EQ(s.size(), 3);
        EXPECT_EQ(os::env("CO_TEST"), "777");
        os::env("CO_TEST", "");
        EXPECT_EQ(os::env("CO_TEST"), co::string());
    }

    DEF_case(homedir) {
        EXPECT_NE(os::homedir(), co::string());
    }

    DEF_case(cwd) {
        auto s = os::cwd();
        EXPECT_NE(s.size(), 0);
        EXPECT_NE(s, co::string());
    }

    DEF_case(exename) {
        EXPECT_NE(os::exepath(), co::string());
        EXPECT_NE(os::exedir(), co::string());
        auto s = os::exename();
        EXPECT_NE(s, co::string());
        EXPECT(os::exepath().starts_with(os::exedir()));
        EXPECT(os::exepath().ends_with(s));
        EXPECT(s.starts_with("unitest"));
    }

    DEF_case(pid) {
        EXPECT_GE(os::pid(), 0);
    }

    DEF_case(cpunum) {
        EXPECT_GT(os::cpunum(), 0);
    }

    DEF_case(cache_line_size) {
        EXPECT_NE(os::cache_line_size(), 0);
    }
}

} // namespace test
