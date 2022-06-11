#include "co/unitest.h"
#include "co/maybe.h"

namespace test {

int gV = 0;
const char* gE = "v <= 0";

co::maybe<void> fvoid(int v) {
    if (v > 0) return 0;
    return co::error(gE);
}

co::maybe<int> fint(int v) {
    if (v > 0) return v;
    return co::error(gE);
}


co::maybe<int&> fref(int v) {
    if (v > 0) return gV;
    return co::error(gE);
}

co::maybe<fastring> fclass(int v) {
    if (v > 0) return fastring("hello");
    return co::error(gE);
}

DEF_test(maybe) {
    DEF_case(void) {
        auto a = fvoid(0);
        auto b = fvoid(1);
        EXPECT(a.has_error());
        EXPECT_EQ(fastring(a.error()), gE);
        EXPECT(!b.has_error());
        EXPECT_EQ(fastring(b.error()), "");
    }

    DEF_case(scalar) {
        auto a = fint(0);
        auto b = fint(1);
        EXPECT(a.has_error());
        EXPECT_EQ(fastring(a.error()), gE);
        EXPECT_EQ(a.value(), 0);
        EXPECT(!b.has_error());
        EXPECT_EQ(fastring(b.error()), "");
        EXPECT_EQ(b.value(), 1);
        EXPECT_EQ(b.must_value(), 1);
    }

    DEF_case(ref) {
        auto a = fref(0);
        auto b = fref(1);
        EXPECT(a.has_error());
        EXPECT_EQ(fastring(a.error()), gE);
        EXPECT(!b.has_error());
        EXPECT_EQ(fastring(b.error()), "");
        EXPECT_EQ(b.value(), 0);
        EXPECT_EQ(b.must_value(), 0);
        b.value() = 3;
        EXPECT_EQ(b.value(), 3);
        EXPECT_EQ(gV, 3);
    }

    DEF_case(class) {
        auto a = fclass(0);
        auto b = fclass(1);
        EXPECT(a.has_error());
        EXPECT_EQ(fastring(a.error()), gE);
        EXPECT(!b.has_error());
        EXPECT_EQ(fastring(b.error()), "");
        EXPECT_EQ(b.value(), "hello");
        EXPECT_EQ(b.must_value(), "hello");
        b.value() = "xxx";
        EXPECT_EQ(b.value(), "xxx");
    }
}

} // test
