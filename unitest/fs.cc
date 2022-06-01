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

    DEF_case(symlink) {
        fs::symlink("xxx", "xxx.lnk");
        //EXPECT(fs::exists("xxx.lnk"));
    }

    DEF_case(remove) {
        EXPECT(fs::remove("xxx"));
        EXPECT(fs::remove("xxx.lnk"));
        EXPECT(fs::remove("xxplus"));
        EXPECT(!fs::remove("xxd"));
        EXPECT(fs::remove("xxd", true));
        EXPECT(!fs::exists("xxx"));
        EXPECT(!fs::exists("xxx.lnk"));
        EXPECT(!fs::exists("xxplus"));
        EXPECT(!fs::exists("xxd"));
    }
}

} // namespace test
