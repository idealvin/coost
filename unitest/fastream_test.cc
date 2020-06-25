#include "co/unitest.h"
#include "co/def.h"
#include "co/fastream.h"

namespace test {

DEF_test(fastream) {
    DEF_case(base) {
        fastream fs;
        EXPECT_EQ(fs.size(), 0);
        EXPECT_EQ(fs.str(), "");
        EXPECT_EQ(std::string(fs.c_str()), "");

        const char* s = "xx";
        fs.append(s, 2);
        EXPECT(!fs.empty());
        EXPECT_EQ(fs.size(), 2);
        EXPECT_EQ(fs.str(), "xx");
        EXPECT_EQ(std::string(fs.c_str()), "xx");

        fastream x(std::move(fs));
        EXPECT_EQ(x.str(), "xx");
        EXPECT_EQ(fs.capacity(), 0);
        EXPECT_EQ(fs.size(), 0);
        EXPECT_EQ(fs.str(), "");

        fs << "xx";
        EXPECT_EQ(fs.size(), 2);
        EXPECT_EQ(fs.str(), "xx");

        fs.clear();
        EXPECT(fs.empty());
        EXPECT_EQ(fs.size(), 0);
        EXPECT_EQ(fs.str(), "");

        size_t cap = fs.capacity();
        fs.reserve(cap - 1);
        EXPECT_EQ(fs.capacity(), cap);

        fs.reserve(cap + 1);
        EXPECT_GT(fs.capacity(), cap);

        fs.resize(1);
        EXPECT_EQ(fs.size(), 1);
        fs.clear();
        EXPECT_EQ(fs.size(), 0);
    }

    DEF_case(bool) {
        fastream fs;
        fs << false << ' ' << true;
        EXPECT_EQ(fs.str(), "false true");
    }

    DEF_case(char) {
        char c = 'c';
        unsigned char uc = 'a';
        fastream fs;
        fs << c << ' ' << uc;
        EXPECT_EQ(fs.str(), "c 97");
    }

    DEF_case(int) {
        short s1 = -1;
        unsigned short s2 = 1;
        int i1 = -2;
        unsigned int i2 = 2;
        long l1 = -4;
        unsigned long l2 = 4;
        long long ll1 = -8;
        unsigned long long ll2 = 8;
        fastream fs;
        fs << s1 << s2 << ' ' << i1 << i2 << ' ' << l1 << l2 << ' ' << ll1 << ll2;
        EXPECT_EQ(fs.str(), "-11 -22 -44 -88");
    }

    DEF_case(float) {
        float f = 3.14f;
        double d = 3.14159;

        fastream fs;
        fs << f;
        EXPECT_NE(fs.str(), "");
        fs.clear();

        fs << d;
        EXPECT_NE(fs.str(), "");
    }

    DEF_case(string) {
        const char* cs = "cs";
        std::string s = "s";
        fastream fs;
        fs << cs << s;
        EXPECT_EQ(fs.str(), "css");
    }

    DEF_case(ptr) {
        int x = 0;
        fastream fs;
        fs << &x;
        EXPECT_NE(fs.str(), "");
        EXPECT_EQ(fs.str().substr(0, 2), "0x");
    }

    DEF_case(binary) {
        int i = 3;
        fastream fs;
        fs.append(i);
        EXPECT_EQ(fs.size(), sizeof(int));
        EXPECT_EQ(*((const int*) fs.data()), 3);
    }

    DEF_case(big_data) {
        std::string s(8192, 'x');
        fastream fs;
        fs << s;
        EXPECT_EQ(fs.size(), s.size());
    }

    DEF_case(swap) {
        fastream fs;
        fastream sd;
        fs << "fs";
        sd << "sd";
        fs.swap(sd);
        EXPECT_EQ(fs.str(), "sd");
        EXPECT_EQ(sd.str(), "fs");
    }

    DEF_case(max_size) {
        fastream fs;
        fs << MAX_UINT64;
        EXPECT_LT(fs.size(), 24);

        fs.clear();
        fs << MIN_INT64;
        EXPECT_LT(fs.size(), 24);

        fs.clear();
        fs << (void*) MAX_UINT64;
        EXPECT_LT(fs.size(), 24);

        fs.clear();
        fs << 1234567890.0123456789876543210;
        EXPECT_LT(fs.size(), 24);
    }
}

} // namespace test
