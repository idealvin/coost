#include "co/unitest.h"
#include "co/str.h"

namespace test {

DEF_test(str) {
    DEF_case(split) {
        auto v = str::split("x||y", '|');
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "x");
        EXPECT_EQ(v[1], "");
        EXPECT_EQ(v[2], "y");

        v = str::split(fastring("x||y"), '|');
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "x");
        EXPECT_EQ(v[1], "");
        EXPECT_EQ(v[2], "y");

        v = str::split("x||y", '|', 1);
        EXPECT_EQ(v[0], "x");
        EXPECT_EQ(v[1], "|y");

        v = str::split("x y", ' ');
        EXPECT_EQ(v.size(), 2);
        EXPECT_EQ(v[0], "x");
        EXPECT_EQ(v[1], "y");

        v = str::split("\nx\ny\n", '\n');
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "");
        EXPECT_EQ(v[1], "x");
        EXPECT_EQ(v[2], "y");

        v = str::split(fastring("\nx\ny\n"), '\n');
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "");
        EXPECT_EQ(v[1], "x");
        EXPECT_EQ(v[2], "y");

        v = str::split("||x||y||", "||");
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "");
        EXPECT_EQ(v[1], "x");
        EXPECT_EQ(v[2], "y");

        v = str::split("||x||y||", "||", 2);
        EXPECT_EQ(v[0], "");
        EXPECT_EQ(v[1], "x");
        EXPECT_EQ(v[2], "y||");
    }

    DEF_case(replace) {
        EXPECT_EQ(str::replace("$@xx$@", "$@", "#"), "#xx#");
        EXPECT_EQ(str::replace("$@xx$@", "$@", "#", 1), "#xx$@");

        fastring s("hello world");
        EXPECT_EQ(str::replace(s, "l", "x"), "hexxo worxd");
        EXPECT_EQ(str::replace(s, "o", "x"), "hellx wxrld");
    }

    DEF_case(strip) {
        EXPECT_EQ(str::strip(" \txx\t  \n"), "xx");
        EXPECT_EQ(str::strip("$@xx@", "$@"), "xx");
        EXPECT_EQ(str::strip("$@xx@", "$@", 'l'), "xx@");
        EXPECT_EQ(str::strip("$@xx@", "$@", 'r'), "$@xx");
        EXPECT_EQ(str::strip("xx", ""), "xx");
        EXPECT_EQ(str::strip("", "xx"), "");
        EXPECT_EQ(str::strip("$@xx@", '$'), "@xx@");
        EXPECT_EQ(str::strip("$@xx@", '$', 'r'), "$@xx@");
        EXPECT_EQ(str::strip("$@xx@", '$', 'l'), "@xx@");
        EXPECT_EQ(str::strip("$@xx@", '@'), "$@xx");
        EXPECT_EQ(str::strip("", '\0'), "");

        EXPECT_EQ(str::strip(fastring("$@xx@"), "$@"), "xx");
        EXPECT_EQ(str::strip(fastring("$@xx@"), "$@", 'l'), "xx@");
        EXPECT_EQ(str::strip(fastring("$@xx@"), "$@", 'r'), "$@xx");
        EXPECT_EQ(str::strip(fastring("$@xx@"), '$'), "@xx@");
        EXPECT_EQ(str::strip(fastring("$@xx@"), '$', 'r'), "$@xx@");
        EXPECT_EQ(str::strip(fastring("$@xx@"), '$', 'l'), "@xx@");
        EXPECT_EQ(str::strip(fastring("$@xx@"), '@'), "$@xx");
        EXPECT_EQ(str::strip(fastring("$@xx@"), fastring("$@")), "xx");
        EXPECT_EQ(str::strip(fastring("$@xx@"), fastring("$@"), 'l'), "xx@");
        EXPECT_EQ(str::strip(fastring("$@xx@"), fastring("$@"), 'r'), "$@xx");
        EXPECT_EQ(str::strip(fastring("\0xx\0", 4), '\0'), "xx");
        EXPECT_EQ(str::strip(fastring("\0xx\0", 4), fastring("x\0", 2)), "");
        EXPECT_EQ(str::strip(fastring("\0xox\0", 5), fastring("x\0", 2), 'l'), fastring("ox\0", 3));
    }

    DEF_case(to) {
        EXPECT_EQ(str::to_bool("true"), true);
        EXPECT_EQ(str::to_bool("1"), true);
        EXPECT_EQ(str::to_bool("false"), false);
        EXPECT_EQ(str::to_bool("0"), false);

        EXPECT_EQ(str::to_int32("-32"), -32);
        EXPECT_EQ(str::to_int32("-4k"), -4096);
        EXPECT_EQ(str::to_int64("-64"), -64);
        EXPECT_EQ(str::to_int64("-8G"), -(8LL << 30));

        EXPECT_EQ(str::to_uint32("32"), 32);
        EXPECT_EQ(str::to_uint32("4K"), 4096);
        EXPECT_EQ(str::to_uint64("64"), 64);
        EXPECT_EQ(str::to_uint64("8t"), 8ULL << 40);

        EXPECT_EQ(str::to_double("3.14159"), 3.14159);

        // convertion failed
        EXPECT_EQ(str::to_bool("xxx"), false);
        EXPECT_EQ(co::error(), EINVAL);

        EXPECT_EQ(str::to_int32("12345678900"), 0);
        EXPECT_EQ(co::error(), ERANGE);
        EXPECT_EQ(str::to_int32("-32g"), 0);
        EXPECT_EQ(co::error(), ERANGE);
        EXPECT_EQ(str::to_int32("-3a2"), 0);
        EXPECT_EQ(co::error(), EINVAL);

        EXPECT_EQ(str::to_int64("1234567890123456789000"), 0);
        EXPECT_EQ(co::error(), ERANGE);
        EXPECT_EQ(str::to_int64("100000P"), 0);
        EXPECT_EQ(co::error(), ERANGE);
        EXPECT_EQ(str::to_int64("1di8"), 0);
        EXPECT_EQ(co::error(), EINVAL);

        EXPECT_EQ(str::to_uint32("123456789000"), 0);
        EXPECT_EQ(co::error(), ERANGE);
        EXPECT_EQ(str::to_uint32("32g"), 0);
        EXPECT_EQ(co::error(), ERANGE);
        EXPECT_EQ(str::to_uint32("3g3"), 0);
        EXPECT_EQ(co::error(), EINVAL);

        EXPECT_EQ(str::to_uint64("1234567890123456789000"), 0);
        EXPECT_EQ(co::error(), ERANGE);
        EXPECT_EQ(str::to_uint64("100000P"), 0);
        EXPECT_EQ(co::error(), ERANGE);
        EXPECT_EQ(str::to_uint64("12d8"), 0);
        EXPECT_EQ(co::error(), EINVAL);

        EXPECT_EQ(str::to_double("3.141d59"), 0);
        EXPECT_EQ(co::error(), EINVAL);

        EXPECT_EQ(str::to_uint32("32"), 32);
        EXPECT_EQ(co::error(), 0);
    }

    DEF_case(from) {
        EXPECT_EQ(str::from(3.14), "3.14");
        EXPECT_EQ(str::from(false), "false");
        EXPECT_EQ(str::from(true), "true");
        EXPECT_EQ(str::from(1024), "1024");
        EXPECT_EQ(str::from(-1024), "-1024");
    }

    DEF_case(dbg) {
        std::vector<fastring> v { "xx", "yy" };
        co::vector<fastring> cv { "xx", "yy" };
        EXPECT_EQ(str::dbg(v),  "[\"xx\",\"yy\"]");
        EXPECT_EQ(str::dbg(cv), "[\"xx\",\"yy\"]");

        std::set<int> s { 7, 0, 3 };
        co::set<int> cs { 7, 0, 3 };
        EXPECT_EQ(str::dbg(s),  "{0,3,7}");
        EXPECT_EQ(str::dbg(cs), "{0,3,7}");

        std::map<int, int> m { {1, 1}, {2, 2}, {3, 3} };
        co::map<int, int> cm { {1, 1}, {2, 2}, {3, 3} };
        EXPECT_EQ(str::dbg(m),  "{1:1,2:2,3:3}");
        EXPECT_EQ(str::dbg(cm), "{1:1,2:2,3:3}");

        std::map<int, fastring> ms {
            {1, "1"}, {2, "2"}, {3, "3"}
        };
        EXPECT_EQ(str::dbg(ms), "{1:\"1\",2:\"2\",3:\"3\"}");
    }

    DEF_case(cat) {
        EXPECT_EQ(str::cat(), "");
        EXPECT_EQ(str::cat(1, 2, 3), "123");
        EXPECT_EQ(str::cat("hello", 1, 2, 3), "hello123");
        EXPECT_EQ(str::cat("hello ", false), "hello false");
        EXPECT_EQ(str::cat("hello ", true, ':', 999), "hello true:999");
        fastring f("fff");
        std::string s("sss");
        const char* c = "ccc";
        EXPECT_EQ(str::cat(f, s, c, 123), "fffsssccc123");
    }
}

} // namespace test
