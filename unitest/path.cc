#include "co/unitest.h"
#include "co/str.h"
#include "co/path.h"

namespace test {

DEF_test(path) {
    DEF_case(clean) {
        EXPECT_EQ(path::clean(""), ".");
        EXPECT_EQ(path::clean(".//x"), "x");
        EXPECT_EQ(path::clean("./x//y/"), "x/y");
        EXPECT_EQ(path::clean("./x/.."), ".");
        EXPECT_EQ(path::clean("./x/../.."), "..");
        EXPECT_EQ(path::clean("./x/../../.."), "../..");
        EXPECT_EQ(path::clean("/x/y/.."), "/x");
        EXPECT_EQ(path::clean("/x/../"), "/");
        EXPECT_EQ(path::clean("/x/./../."), "/");
        EXPECT_EQ(path::clean("/x/../.."), "/");
      #ifdef _WIN32
        EXPECT_EQ(path::clean("C:"), "C:");
        EXPECT_EQ(path::clean("C:/"), "C:/");
        EXPECT_EQ(path::clean("C://x//y/"), "C:/x/y");
        EXPECT_EQ(path::clean("C:/x/../"), "C:/");
        EXPECT_EQ(path::clean("C:/x/./../."), "C:/");
        EXPECT_EQ(path::clean("C:/x/../.."), "C:/");
      #endif
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
        EXPECT_EQ(path::join("D:", "x"), "D:/x");
        EXPECT_EQ(path::join("D:/x/", "y"), "D:/x/y");
        EXPECT_EQ(path::join("x", fastring("y"), std::string("z")), "x/y/z");
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
      #ifdef _WIN32
        EXPECT(path::split("C:") == Pair("C:", ""));
        EXPECT(path::split("C:/") == Pair("C:/", ""));
        EXPECT(path::split("C:/a") == Pair("C:/", "a"));
        EXPECT(path::split("C:/a/b") == Pair("C:/a/", "b"));
      #endif
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
      #ifdef _WIN32
        EXPECT_EQ(path::dir("C:"), "C:");
        EXPECT_EQ(path::dir("C:/"), "C:/");
        EXPECT_EQ(path::dir("C:/a"), "C:/");
        EXPECT_EQ(path::dir("C:/a/b"), "C:/a");
      #endif
    }

    DEF_case(base) {
        EXPECT_EQ(path::base(""), ".");
        EXPECT_EQ(path::base("/"), "/");
        EXPECT_EQ(path::base("/a/"), "a");
        EXPECT_EQ(path::base("a"), "a");
        EXPECT_EQ(path::base("/a"), "a");
        EXPECT_EQ(path::base("/a/b"), "b");
      #ifdef _WIN32
        EXPECT_EQ(path::base("C:"), "C:");
        EXPECT_EQ(path::base("C:/"), "C:/");
        EXPECT_EQ(path::base("C:/a"), "a");
        EXPECT_EQ(path::base("C:/a/b"), "b");
      #endif
    }

    DEF_case(ext) {
        EXPECT_EQ(path::ext(""), "");
        EXPECT_EQ(path::ext("/"), "");
        EXPECT_EQ(path::ext("/a"), "");
        EXPECT_EQ(path::ext("/a.cc"), ".cc");
        EXPECT_EQ(path::ext("/a.cc/"), "");
        EXPECT_EQ(path::ext("/a/b.cc"), ".cc");
        EXPECT_EQ(path::ext("C:/a/b.cc"), ".cc");
    }
}

} // namespace test
