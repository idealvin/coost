#include "base/unitest.h"
#include "base/str.h"
#include "base/path.h"

namespace test {

DEF_test(path) {
    DEF_case(clean) {
        EXPECT_EQ(path::clean(".//x"), "x");
        EXPECT_EQ(path::clean("./x//y/"), "x/y");
        EXPECT_EQ(path::clean("./x/.."), ".");
        EXPECT_EQ(path::clean("./x/../.."), "..");
        EXPECT_EQ(path::clean("./x/../../.."), "../..");
        EXPECT_EQ(path::clean("/x/y/.."), "/x");
        EXPECT_EQ(path::clean("/x/../"), "/");
        EXPECT_EQ(path::clean("/x/./../."), "/");
        EXPECT_EQ(path::clean("/x/../.."), "/");
    }

    DEF_case(join) {
        EXPECT_EQ(path::join(""), "");
        EXPECT_EQ(path::join("", ""), "");
        EXPECT_EQ(path::join("", "x"), "x");
        EXPECT_EQ(path::join("x", ""), "x");
        EXPECT_EQ(path::join("", "/x"), "/x");
        EXPECT_EQ(path::join("/x", "y"), "/x/y");
        EXPECT_EQ(path::join("/x/", "y"), "/x/y");
        EXPECT_EQ(path::join("/x/", "y", "z"), "/x/y/z");
        EXPECT_EQ(path::join("x", "", "y"), "x/y");
    }

    DEF_case(split) {
        typedef std::pair<fastring, fastring> Pair;

        EXPECT(path::split("") == Pair("", ""));
        EXPECT(path::split("a") == Pair("", "a"));
        EXPECT(path::split("/") == Pair("/", ""));
        EXPECT(path::split("/a") == Pair("/", "a"));
        EXPECT(path::split("/a/") == Pair("/a/", ""));
        EXPECT(path::split("/a/b") == Pair("/a/", "b"));
        EXPECT(path::split("/a/b/c") == Pair("/a/b/", "c"));
    }

    DEF_case(dir) {
        EXPECT_EQ(path::dir(""), ".");
        EXPECT_EQ(path::dir("a"), ".");
        EXPECT_EQ(path::dir("a/"), "a");
        EXPECT_EQ(path::dir("/"), "/");
        EXPECT_EQ(path::dir("/a"), "/");
        EXPECT_EQ(path::dir("/a/"), "/a");
        EXPECT_EQ(path::dir("/a/b"), "/a");
        EXPECT_EQ(path::dir("/a/b/c"), "/a/b");
    }

    DEF_case(base) {
        EXPECT_EQ(path::base(""), ".");
        EXPECT_EQ(path::base("/"), "/");
        EXPECT_EQ(path::base("/a/"), "a");
        EXPECT_EQ(path::base("a"), "a");
        EXPECT_EQ(path::base("/a"), "a");
        EXPECT_EQ(path::base("/a/b"), "b");
    }

    DEF_case(ext) {
        EXPECT_EQ(path::ext(""), "");
        EXPECT_EQ(path::ext("/"), "");
        EXPECT_EQ(path::ext("/a"), "");
        EXPECT_EQ(path::ext("/a.cc"), ".cc");
        EXPECT_EQ(path::ext("/a.cc/"), "");
        EXPECT_EQ(path::ext("/a/b.cc"), ".cc");
    }
}

} // namespace test
