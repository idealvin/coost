#include "base/unitest.h"
#include "base/str.h"

namespace test {

DEF_test(str) {
    DEF_case(split) {
        auto v = str::split("x||y", '|');
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "x");
        EXPECT_EQ(v[1], "");
        EXPECT_EQ(v[2], "y");

        auto u = str::split(fastring("x||y"), '|');
        EXPECT(u == v);

        v = str::split("x||y", '|', 1);
        EXPECT_EQ(v[0], "x");
        EXPECT_EQ(v[1], "|y");

        u = str::split(fastring("x||y"), '|', 1);
        EXPECT(u == v);

        v = str::split("x y", ' ');
        EXPECT_EQ(v.size(), 2);
        EXPECT_EQ(v[0], "x");
        EXPECT_EQ(v[1], "y");

        v = str::split("\nx\ny\n", '\n');
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "");
        EXPECT_EQ(v[1], "x");
        EXPECT_EQ(v[2], "y");

        u = str::split(fastring("\nx\ny\n"), '\n');
        EXPECT(u == v);

        v = str::split("||x||y||", "||");
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "");
        EXPECT_EQ(v[1], "x");
        EXPECT_EQ(v[2], "y");

        u = str::split(fastring("||x||y||"), "||");
        EXPECT(u == v);

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

        fastring t(s);
        s = str::replace(t, "xxx", "yy");
        EXPECT_EQ(s.data(), t.data());
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
 
        fastring s("$@xx@");
        fastring t(s);
        s = str::strip(t, "xx");
        EXPECT_EQ(s.data(), t.data());
        s = str::strip(t, fastring("xx"));
        EXPECT_EQ(s.data(), t.data());
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
    }

    DEF_case(from) {
        EXPECT_EQ(str::from(3.14), "3.14");
        EXPECT_EQ(str::from(false), "false");
        EXPECT_EQ(str::from(true), "true");
        EXPECT_EQ(str::from(1024), "1024");
        EXPECT_EQ(str::from(-1024), "-1024");
    }

    DEF_case(dbg) {
        std::vector<fastring> v {
            "xx", "yy"
        };
        EXPECT_EQ(str::dbg(v), "[\"xx\",\"yy\"]");

        std::set<int> s {
            7, 0, 3
        };
        EXPECT_EQ(str::dbg(s), "{0,3,7}");

        std::map<int, int> m {
            {1, 1}, {2, 2}, {3, 3}
        };
        EXPECT_EQ(str::dbg(m), "{1:1,2:2,3:3}");

        std::map<int, fastring> ms {
            {1, "1"}, {2, "2"}, {3, "3"}
        };
        EXPECT_EQ(str::dbg(ms), "{1:\"1\",2:\"2\",3:\"3\"}");
    }
}

} // namespace test
