#include "co/unitest.h"
#include "co/fs.h"

namespace test {

DEF_test(fs) {
    DEF_case(file) {
        fs::file fx(128);
        EXPECT_EQ(co::string(), fx.path());

        fs::file fo("xxx", 'w');
        EXPECT(fo);
        fo.write("99999");
        EXPECT_EQ(fo.size(), 5);
        EXPECT_EQ(co::string("xxx"), fo.path());
        fo.close();

        EXPECT(!fo.open("", 'a'));
        fo.open("xxx", 'm');
        EXPECT(fo);
        EXPECT_EQ(fo.size(), 5);
        fo.close();

        fo.open("xxx", 'w');
        fo.write("1234567");
        fo.write('x');
        fo.close();
         
        fo.open("xxx", 'm');
        EXPECT_EQ(fo.size(), 8);

        fo.seek(7);
        fo.write('8');

        fo.open("xxx", 'r');
        char buf[32];
        size_t r = fo.read(buf, 32);
        EXPECT_EQ(co::string(buf, r), "12345678");
        fo.close();

        fo.open("xxx", 'a');
        fo.write("90");
        fo.close();

        fo.open("xxx", 'r');
        r = fo.read(buf, 32);
        EXPECT_EQ(co::string(buf, r), "1234567890");

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
        EXPECT_EQ(co::string(buf, 11), "hello123456");
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

    DEF_case(dir) {
        EXPECT(fs::mkdir("xxreaddir"));
        fs::file x("xxreaddir/xx.txt", 'w');
        fs::file y("xxreaddir/yy.txt", 'w');
        EXPECT(x);
        EXPECT(y);
        x.close();
        y.close();

        fs::dir d("xxreaddir");
        auto v = d.all();
        EXPECT_EQ(v.size(), 2)
        if (v[0] < v[1]) {
            EXPECT_EQ(v[0], "xx.txt")
            EXPECT_EQ(v[1], "yy.txt")
        } else {
            EXPECT_EQ(v[0], "yy.txt")
            EXPECT_EQ(v[1], "xx.txt")
        }
        d.close();

        d.open("xxreaddir");
        v.clear();
        for (auto it = d.begin(); it != d.end(); ++it) {
            v.push_back(*it);
        }
        EXPECT_EQ(v.size(), 2)
        if (v[0] < v[1]) {
            EXPECT_EQ(v[0], "xx.txt")
            EXPECT_EQ(v[1], "yy.txt")
        } else {
            EXPECT_EQ(v[0], "yy.txt")
            EXPECT_EQ(v[1], "xx.txt")
        }

        fs::rm("xxreaddir", true);
        EXPECT(!fs::exists("xxreaddir"))
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
        fs::rm("xxd/xxx");

        EXPECT_EQ(fs::mkdir("xxs"), true);
        EXPECT_EQ(fs::mkdir("xxd/xxs"), true);
        EXPECT_EQ(fs::mv("xxs", "xxd"), true);
        EXPECT(!fs::exists("xxs"));

        EXPECT_EQ(fs::mkdir("xxd/xxs/xx"), true);
        EXPECT_EQ(fs::mkdir("xxs"), true);
        EXPECT_EQ(fs::mv("xxs", "xxd"), false);

        fs::rm("xxd/xxs", true);
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

    DEF_case(rm) {
        EXPECT(fs::rm("xxx"));
        EXPECT(fs::rm("xxx.lnk"));
        EXPECT(fs::rm("xxd.lnk"));
        EXPECT(fs::rm("xxplus"));
        EXPECT(!fs::rm("xxd"));
        EXPECT(fs::rm("xxd", true));
        EXPECT(fs::rm("xxs"));
        EXPECT(!fs::exists("xxx"));
        EXPECT(!fs::exists("xxx.lnk"));
        EXPECT(!fs::exists("xxd.lnk"));
        EXPECT(!fs::exists("xxplus"));
        EXPECT(!fs::exists("xxs"));
        EXPECT(!fs::exists("xxd"));
    }
}

} // namespace test
