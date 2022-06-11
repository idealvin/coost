#include "co/unitest.h"
#include "co/os.h"

namespace test {

DEF_test(os) {
    DEF_case(env) {
        EXPECT_EQ(os::env("CO_TEST"), fastring());
        os::env("CO_TEST", "777");
        EXPECT_EQ(os::env("CO_TEST"), "777");
        os::env("CO_TEST", "");
        EXPECT_EQ(os::env("CO_TEST"), fastring());
    }

    DEF_case(homedir) {
        EXPECT_NE(os::homedir(), fastring());
    }

    DEF_case(cwd) {
        EXPECT_NE(os::cwd(), fastring());
    }

    DEF_case(exename) {
        EXPECT_NE(os::exepath(), fastring());
        EXPECT_NE(os::exedir(), fastring());
        EXPECT_NE(os::exename(), fastring());
        EXPECT(os::exepath().starts_with(os::exedir()));
        EXPECT(os::exepath().ends_with(os::exename()));
        EXPECT(os::exename().starts_with("unitest"));
    }

    DEF_case(pid) {
        EXPECT_GE(os::pid(), 0);
    }

    DEF_case(cpunum) {
        EXPECT_GT(os::cpunum(), 0);
    }
}

} // namespace test
