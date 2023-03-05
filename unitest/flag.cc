#include "co/unitest.h"
#include "co/flag.h"
#include "co/fs.h"

DEF_bool(ut_bool, false, "bool type");
DEF_int32(ut_int32, -32, "int32 type");
DEF_int64(ut_int64, -64, "int64 type");
DEF_uint32(ut_uint32, 32, "uint32 type");
DEF_uint64(ut_uint64, 64, "uint64 type");
DEF_double(ut_double, 2.3, "double type");
DEF_string(ut_string, "", "string type");

namespace test {

DEF_test(flag) {
    DEF_case(default) {
        EXPECT_EQ(FLG_ut_bool, false);
        EXPECT_EQ(FLG_ut_int32, -32);
        EXPECT_EQ(FLG_ut_int64, -64);
        EXPECT_EQ(FLG_ut_uint32, 32);
        EXPECT_EQ(FLG_ut_uint64, 64);
        EXPECT_EQ(FLG_ut_double, 2.3);
        EXPECT_EQ(FLG_ut_string, "");
    }

    DEF_case(_xx_space_xx) {
        int argc = 15;
        const char* argv[15] = {
            "xxx",
            "-ut_bool", "true",
            "-ut_int32", "-4K",
            "-ut_int64", "-8G",
            "-ut_uint32", "4M",
            "-ut_uint64", "8T",
            "-ut_double", "3.14",
            "-ut_string", "again",
        };
        flag::parse(argc, (char**)argv);

        EXPECT_EQ(FLG_ut_bool, true);
        EXPECT_EQ(FLG_ut_int32, -4096);
        EXPECT_EQ(FLG_ut_int64, -(8LL << 30));
        EXPECT_EQ(FLG_ut_uint32, 4U << 20);
        EXPECT_EQ(FLG_ut_uint64, 8ULL << 40);
        EXPECT_EQ(FLG_ut_double, 3.14);
        EXPECT_EQ(FLG_ut_string, "again");
    }

    DEF_case(_xx_eq_xx) {
        int argc = 8;
        const char* argv[8] = {
            "xxx",
            "-ut_bool=false",
            "-ut_int32=4k",
            "-ut_int64=8g",
            "-ut_uint32=32",
            "-ut_uint64=64",
            "-ut_double=0.23",
            "-ut_string=hello",
        };
        flag::parse(argc, (char**)argv);

        EXPECT_EQ(FLG_ut_bool, false);
        EXPECT_EQ(FLG_ut_int32, 4096);
        EXPECT_EQ(FLG_ut_int64, (8LL << 30));
        EXPECT_EQ(FLG_ut_uint32, 32);
        EXPECT_EQ(FLG_ut_uint64, 64);
        EXPECT_EQ(FLG_ut_double, 0.23);
        EXPECT_EQ(FLG_ut_string, "hello");
    }

    DEF_case(xx_eq_xx) {
        int argc = 8;
        const char* argv[8] = {
            "xxx",
            "ut_bool=true",
            "ut_int32=-4k",
            "ut_int64=-8g",
            "ut_uint32=4m",
            "ut_uint64=8t",
            "ut_double=3.14",
            "ut_string=what",
        };
        flag::parse(argc, (char**)argv);

        EXPECT_EQ(FLG_ut_bool, true);
        EXPECT_EQ(FLG_ut_int32, -4096);
        EXPECT_EQ(FLG_ut_int64, -(8LL << 30));
        EXPECT_EQ(FLG_ut_uint32, 4U << 20);
        EXPECT_EQ(FLG_ut_uint64, 8ULL << 40);
        EXPECT_EQ(FLG_ut_double, 3.14);
        EXPECT_EQ(FLG_ut_string, "what");
    }

    DEF_case(config) {
        fs::fstream s("ut_xxx.conf", 'w');
        if (s) {
            s << "ut_bool   = false" << '\n'
              << "ut_int32  = 888" << '\n'
              << "ut_int64  = -4k" << '\n'
              << "ut_uint32 = 2g" << '\n'
              << "ut_uint64 = 8M" << '\n'
              << "ut_double = 9.9" << '\n'
              << "ut_string = hello world" << '\n';
            s.close();

            flag::parse("ut_xxx.conf");

            EXPECT_EQ(FLG_ut_bool, false);
            EXPECT_EQ(FLG_ut_int32, 888);
            EXPECT_EQ(FLG_ut_int64, -4096);
            EXPECT_EQ(FLG_ut_uint32, 2U << 30);
            EXPECT_EQ(FLG_ut_uint64, 8ULL << 20);
            EXPECT_EQ(FLG_ut_double, 9.9);
            EXPECT_EQ(FLG_ut_string, "hello world");

            fs::remove("ut_xxx.conf");
        }
    }
}

} // test
