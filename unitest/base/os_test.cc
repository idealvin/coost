#include "base/unitest.h"
#include "base/os.h"

namespace test {

DEF_test(os) {
    DEF_case(homedir) {
        EXPECT_NE(os::homedir(), fastring());
    }

    DEF_case(cwd) {
        EXPECT_NE(os::cwd(), fastring());
    }

    DEF_case(exename) {
        EXPECT_NE(os::exename(), fastring());
        EXPECT_NE(os::exepath(), fastring());
        EXPECT(os::exename().starts_with("unitest"));
        EXPECT(os::exepath().ends_with(os::exename()));
    }

    DEF_case(pid) {
        EXPECT_GE(os::pid(), 0);
    }

    DEF_case(cpunum) {
        EXPECT_GT(os::cpunum(), 0);
    }
}

} // namespace test
