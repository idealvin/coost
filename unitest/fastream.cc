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

        s = x.data();
        EXPECT_EQ(x.str(), "xx");
        EXPECT_EQ(x.data(), s);

        fs << "xx";
        EXPECT_EQ(fs.size(), 2);
        EXPECT_EQ(fs.str(), "xx");

        fs.append(s, 0);
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

        fs.reset();
        EXPECT_EQ(fs.capacity(), 0);
    }

    DEF_case(cat) {
        fastream s;
        EXPECT_EQ(s.cat().str(), "");
        EXPECT_EQ(s.cat(1, 2, 3).str(), "123");
        EXPECT_EQ(s.cat(' ', "hello ", false).str(), "123 hello false");
    }

    DEF_case(safe) {
        fastream s(16);
        s << "1234567890";
        s.append(s.data() + 1, 8);
        EXPECT_EQ(s.str(), "123456789023456789");

        s.clear(0);
        EXPECT_EQ(*s.data(), 0);
        EXPECT_EQ(*(s.data() + 3), 0);

        s.append("12345678");
        s.resize(6);
        s.resize(10);
        EXPECT_EQ(s[6], '7');
        EXPECT_EQ(s[7], '8');
        s.resize(6);
        s.resize(10, 0);
        EXPECT_EQ(s[6], 0);
        EXPECT_EQ(s[7], 0);
    }

    DEF_case(bool) {
        fastream fs;
        fs << false << ' ' << true;
        EXPECT_EQ(fs.str(), "false true");
        EXPECT_EQ((fastream() << false << ' ' << true).str(), "false true");
    }

    DEF_case(char) {
        char c = 'c';
        signed char sc = 'c';
        unsigned char uc = 'c';
        const char& x = c;
        fastream fs;
        fs << c << sc << uc << x;
        EXPECT_EQ(fs.str(), "cccc");
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
        float f = 0.5f;
        double d = 3.14159;

        fastream fs;
        fs << f;
        EXPECT_NE(fs.str(), "");
        EXPECT_EQ(fs.str(), "0.5");
        fs.clear();

        fs << d;
        EXPECT_NE(fs.str(), "");
        EXPECT_EQ(fs.str(), "3.14159");

        fs.clear();
        fs << dp::_3(d);
        EXPECT_EQ(fs.str(), "3.141");

        fs.clear();
        fs << dp::_2(d);
        EXPECT_EQ(fs.str(), "3.14");

        fs.clear();
        fs << dp::_n(d, 4);
        EXPECT_EQ(fs.str(), "3.1415");
    }

    DEF_case(string) {
        const char* cs = "cs";
        std::string s = "s";
        fastring x = "x";
        char yz[4];
        yz[0] = 'y';
        yz[1] = 'z';
        yz[2] = '\0';
        yz[3] = 'x';

        fastream fs;
        fs << cs << s << x << yz << "^o^";
        EXPECT_EQ(fs.str(), "cssxyz^o^");
        EXPECT_EQ((fastream() << cs << s << x << yz << "^o^").str(), "cssxyz^o^");

        fs.clear();
        fs << fs;
        EXPECT_EQ(fs.str(), "");

        fs.reset();
        fs << "xx";
        EXPECT_EQ(fs.capacity(), 3);

        fs << fs;
        EXPECT_EQ(fs.str(), "xxxx");
        EXPECT_EQ(fs.capacity(), 5);
    }

    DEF_case(ptr) {
        int x = 0;
        fastream fs;
        fs << &x;
        EXPECT_NE(fs.str(), "");
        EXPECT_EQ(fs.str().substr(0, 2), "0x");
    }

    DEF_case(binary) {
        uint16 u16 = 16;
        uint32 u32 = 32;
        uint64 u64 = 64;

        fastream fs;
        fs.append(u16);
        EXPECT_EQ(fs.size(), sizeof(u16));
        EXPECT_EQ(*((const uint16*) fs.data()), 16);

        fs.clear();
        fs.append(u32);
        EXPECT_EQ(fs.size(), sizeof(u32));
        EXPECT_EQ(*((const uint32*) fs.data()), 32);

        fs.clear();
        fs.append(u64);
        EXPECT_EQ(fs.size(), sizeof(u64));
        EXPECT_EQ(*((const uint64*) fs.data()), 64);

        fs.append(fs);
        EXPECT_EQ(fs.size(), sizeof(u64) * 2);
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

        fs.swap(fastream());
        EXPECT_EQ(fs.capacity(), 0);
        EXPECT_EQ(fs.size(), 0);
        EXPECT_EQ(((size_t)fs.data()), ((size_t)0));
        EXPECT_EQ(fs.str(), "");
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
