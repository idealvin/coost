#include "co/unitest.h"
#include "co/fs.h"

namespace test {

DEF_test(fs) {
    DEF_case(file) {
        fs::file fx(128);
        EXPECT_EQ(fastring(), fx.path());

        fs::file fo("xxx", 'w');
        EXPECT(fo);
        fo.write("99999");
        EXPECT_EQ(fo.size(), 5);
        EXPECT_EQ(fastring("xxx"), fo.path());
        fo.close();

        EXPECT(!fo.open("", 'a'));
        fo.open("xxx", 'm');
        EXPECT(fo);
        EXPECT_EQ(fo.size(), 5);
        fo.close();

        fs::fstream s;
        s.open("xxx", 'w');
        s << 1234567 << 'x';
        s.flush();
        s.close();

        fo.open("xxx", 'm');
        EXPECT_EQ(fo.size(), 8);

        fo.seek(7);
        fo.write('8');

        fo.open("xxx", 'r');
        char buf[32];
        size_t r = fo.read(buf, 32);
        EXPECT_EQ(fastring(buf, r), "12345678");
        fo.close();

        s.open("xxx", 'a');
        s << 90;
        s.flush();

        fo.open("xxx", 'r');
        r = fo.read(buf, 32);
        EXPECT_EQ(fastring(buf, r), "1234567890");

        fo.open("xxplus", '+');
        fo.seek(0);
        fo.write("hello123");

        fo.seek(0);
        r = fo.read(buf, 8);
        EXPECT_EQ(r, 8);

        fo.seek(fo.size()); // seek to tail
        fo.write("456");

        fo.seek(8);
        r = fo.read(buf + 8, 8);
        EXPECT_EQ(r, 3);
        EXPECT_EQ(fastring(buf, 11), "hello123456");
    }

    DEF_case(attr) {
        EXPECT(fs::exists("."));
        EXPECT(fs::exists("xxx"));
        EXPECT(!fs::isdir("xxx"));
        EXPECT_NE(fs::mtime("xxx"), -1);
        EXPECT_EQ(fs::fsize("xxx"), 10);
    }

    DEF_case(mkdir) {
        EXPECT(fs::mkdir("xxd"));
        EXPECT(fs::exists("xxd"));
        EXPECT(fs::mkdir("xxd/a/b", true));
        EXPECT(fs::exists("xxd/a/b"));
    }

    DEF_case(rename) {
        fs::rename("xxx", "yyy");
        EXPECT(!fs::exists("xxx"));
        EXPECT(fs::exists("yyy"));
        fs::rename("yyy", "xxx");
    }

    DEF_case(mv) {
        fs::mv("xxx", "yyy");
        EXPECT(!fs::exists("xxx"));
        EXPECT(fs::exists("yyy"));
        fs::mv("yyy", "xxx");

        fs::mv("xxx", "xxd");
        EXPECT(!fs::exists("xxx"));
        EXPECT(fs::exists("xxd/xxx"));
        fs::mv("xxd/xxx", "xxx");
        EXPECT(!fs::exists("xxd/xxx"));

        fs::file o("xxd/xxx", 'w');
        o.close();
        EXPECT_EQ(fs::mv("xxx", "xxd"), true);
        EXPECT_NE(fs::fsize("xxd/xxx"), 0);
        fs::mv("xxd/xxx", "xxx");

        fs::mkdir("xxd/xxx");
        EXPECT_EQ(fs::mv("xxx", "xxd"), false);
        fs::remove("xxd/xxx");

        EXPECT_EQ(fs::mkdir("xxs"), true);
        EXPECT_EQ(fs::mkdir("xxd/xxs"), true);
        EXPECT_EQ(fs::mv("xxs", "xxd"), true);
        EXPECT(!fs::exists("xxs"));

        EXPECT_EQ(fs::mkdir("xxd/xxs/xx"), true);
        EXPECT_EQ(fs::mkdir("xxs"), true);
        EXPECT_EQ(fs::mv("xxs", "xxd"), false);

        fs::remove("xxd/xxs", true);
        EXPECT(!fs::exists("xxd/xxs"));

        o.open("xxd/xxs", 'w');
        o.close();
        EXPECT_EQ(fs::mv("xxs", "xxd"), false);
        EXPECT_EQ(fs::mv("xxs", "xxd/xxs"), false);
    }

  #ifndef _WIN32
    DEF_case(symlink) {
        fs::symlink("xxx", "xxx.lnk");
        fs::symlink("xxd", "xxd.lnk");
        EXPECT(fs::exists("xxx.lnk"));
        EXPECT(fs::exists("xxd.lnk"));
        EXPECT_EQ(fs::symlink("xxx", "xxx.lnk"), true);
        EXPECT_EQ(fs::symlink("xxd", "xxd.lnk"), true);
    }
  #endif

    DEF_case(remove) {
        EXPECT(fs::remove("xxx"));
        EXPECT(fs::remove("xxx.lnk"));
        EXPECT(fs::remove("xxd.lnk"));
        EXPECT(fs::remove("xxplus"));
        EXPECT(!fs::remove("xxd"));
        EXPECT(fs::remove("xxd", true));
        EXPECT(fs::remove("xxs"));
        EXPECT(!fs::exists("xxx"));
        EXPECT(!fs::exists("xxx.lnk"));
        EXPECT(!fs::exists("xxd.lnk"));
        EXPECT(!fs::exists("xxplus"));
        EXPECT(!fs::exists("xxs"));
        EXPECT(!fs::exists("xxd"));
    }
}

} // namespace test
