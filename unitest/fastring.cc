#include "co/unitest.h"
#include "co/fastring.h"

namespace test {

DEF_test(fastring) {
    DEF_case(constructor) {
        {
            fastring s;
            EXPECT(s.empty());
            EXPECT_EQ(s.size(), 0);
        }
        {
            fastring s(32);
            EXPECT(s.empty());
            EXPECT_EQ(s.capacity(), 32);
        }
        {
            fastring s(4, 'x');
            EXPECT_EQ(s.size(), 4);
            EXPECT_EQ(s.capacity(), 5);
            EXPECT_EQ(s, "xxxx");
        }
        {
            fastring s(nullptr, 0);
            EXPECT(s.empty());
            EXPECT_EQ(s.capacity(), 0);
        }
        {
            fastring s("helloworld", 5);
            EXPECT_EQ(s.size(), 5);
            EXPECT_EQ(s, "hello");
            EXPECT_LT(s.size(), s.capacity());
        }
        {
            fastring s("helloworld");
            EXPECT_EQ(s.size(), 10);
            EXPECT_EQ(s, "helloworld");
            EXPECT_LT(s.size(), s.capacity());
        }
    }

    DEF_case(base) {
        {
            fastring s;
            EXPECT(s.empty());
            EXPECT_EQ(s.size(), 0);        
            s = "xxx";
            EXPECT_EQ(s, "xxx");

            s.push_back('o');
            EXPECT_EQ(s, "xxxo");
            EXPECT_EQ(s.pop_back(), 'o');
            EXPECT_EQ(s.size(), 3);
        }

        {
            fastring s("");
            EXPECT(s.data() == 0);
            EXPECT_EQ(s.size(), 0);
            EXPECT_EQ(s.capacity(), 0);
        }

        {
            fastring s("888");
            EXPECT_EQ(s, "888");
            EXPECT_EQ(s.size(), 3);
        }

        fastring x(std::string("888"));
        EXPECT_EQ(x, "888");

        fastring y = std::move(x);
        EXPECT_EQ(y, "888");
        EXPECT_EQ(x, "");
        EXPECT_EQ(x.capacity(), 0);
        EXPECT_EQ(x.size(), 0);
        EXPECT_EQ(((size_t)x.data()), ((size_t)0));

        {
            fastring s(y.c_str());
            EXPECT_EQ(s, "888");
        }
        {
            y.clear();
            fastring s(y.c_str());
            EXPECT(s.data() == 0);
            EXPECT_EQ(s.size(), 0);
        }
    }

    DEF_case(append) {
        fastring s;
        s.append(s);
        EXPECT_EQ(s, "");

        s.append(3, 'x');
        EXPECT_EQ(s.size(), 3);
        EXPECT_EQ(s, "xxx");
        EXPECT_LT(s.size(), s.capacity());

        s.append(s); // append self
        EXPECT_EQ(s, "xxxxxx");

        s.resize(5);
        EXPECT_EQ(s, "xxxxx");

        s = s.c_str() + 1;
        EXPECT_EQ(s, "xxxx");

        s.append(s.c_str() + 2);
        EXPECT_EQ(s, "xxxxxx");

        s.resize(3);

        s += "yyy";
        EXPECT_EQ(s, "xxxyyy");

        s += "zzz";
        EXPECT_EQ(s, "xxxyyyzzz");

        fastring s1 = s + "...";
        EXPECT_EQ(s1, "xxxyyyzzz...");

        s1 = s1 + "...";
        EXPECT_EQ(s1, "xxxyyyzzz......");

        s.swap(fastring());
        s.append('x');
        EXPECT_EQ(s, "x");

        std::string x("xx");
        s.append(x);
        EXPECT_EQ(s, "xxx");

        uint8 u = 'x';
        s.append(u);
        EXPECT_EQ(s, "xxxx");

        char c = 'x';
        s.append(c);
        EXPECT_EQ(s, "xxxxx");

        EXPECT_EQ(fastring("x") + 'x', "xx");
        EXPECT_EQ('x' + fastring("x"), "xx");
        EXPECT_EQ(fastring("x") + std::string("x"), "xx");
        EXPECT_EQ(std::string("x") + fastring("x"), "xx");
    }

    DEF_case(assign) {
        fastring s;
        s = "hello";
        EXPECT_EQ(s, "hello");

        s.assign("12345");
        EXPECT_EQ(s, "12345");

        s.assign(s.data() + 1, 4);
        EXPECT_EQ(s, "2345");

        s = s.c_str() + 1;
        EXPECT_EQ(s, "345");

        s.assign(3, '0');
        EXPECT_EQ(s, "000");

        s.assign(31, '0');
        EXPECT_EQ(s, fastring(31, '0'));
    }

    DEF_case(cat) {
        fastring s;
        EXPECT_EQ(s.cat(), "");
        EXPECT_EQ(s.cat(1, 2, 3), "123");
        EXPECT_EQ(s.cat(' ', "hello ", false), "123 hello false");
    }

    DEF_case(c_str) {
        {
            fastring s;
            EXPECT_EQ(fastring(s.c_str()), "");
        }
        {
            fastring s("xxx", 3);
            EXPECT_LT(s.size(), s.capacity());
            const char* p = s.data();
            EXPECT_EQ(p, s.c_str())

            fastring x(7, 'x');
            EXPECT_LT(x.size(), x.capacity());

            s.assign(x.data(), x.size());
            EXPECT_LT(s.size(), s.capacity());
            EXPECT_EQ(s, "xxxxxxx");
            EXPECT_EQ(s.capacity(), 8);

            s.append('x');
            EXPECT_LT(s.size(), s.capacity());
            p = s.data();
            EXPECT_EQ(p, s.c_str())

            s.append(s.capacity() - s.size(), 'x');
            EXPECT_LT(s.size(), s.capacity());
            p = s.data();
            EXPECT_EQ(p, s.c_str())

            x.assign(s.capacity() - s.size() + 3, 'x');
            s.append(x.data(), x.size());
            EXPECT_LT(s.size(), s.capacity());
            p = s.data();
            EXPECT_EQ(p, s.c_str());
        }
        {
            const char* p;
            fastring s;
            s << false;
            EXPECT_LT(s.size(), s.capacity());

            s.reset();
            s << 1234567;
            EXPECT_LT(s.size(), s.capacity());

            s.reset();
            s << 3.1415926;
            EXPECT_LT(s.size(), s.capacity());

            p = s.data();
            EXPECT_EQ(p, s.c_str());
        }
    }

    DEF_case(operator<<) {
        fastring s;
        {
            s << false << ' ' << true;
            EXPECT_EQ(s, "false true");
            EXPECT_EQ((fastring() << false << ' ' << true), "false true");
            s.clear();
        }

        {
            char c = 'c';
            signed char sc = 'c';
            unsigned char uc = 'c';
            const char& x = c;
            s << c << sc << uc << x;
            EXPECT_EQ(s, "cccc");
            s.clear();
        }

        {
            short s1 = -1;
            unsigned short s2 = 1;
            int i1 = -2;
            unsigned int i2 = 2;
            long l1 = -4;
            unsigned long l2 = 4;
            long long ll1 = -8;
            unsigned long long ll2 = 8;
            s << s1 << s2 << ' ' << i1 << i2 << ' ' << l1 << l2 << ' ' << ll1 << ll2;
            EXPECT_EQ(s, "-11 -22 -44 -88");
            s.clear();
        }

        {
            float f = 0.5f;
            double d = 3.14159;

            s << f;
            EXPECT_NE(s, "");
            EXPECT_EQ(s, "0.5");
            s.clear();

            s << d;
            EXPECT_NE(s, "");
            EXPECT_EQ(s, "3.14159");
            s.clear();
        }

        {
            const char* cs = "cs";
            std::string ss = "ss";
            fastring x = "x";
            char yz[4];
            yz[0] = 'y';
            yz[1] = 'z';
            yz[2] = '\0';
            yz[3] = 'x';

            s << cs << ss << x << yz << "^o^";
            EXPECT_EQ(s, "csssxyz^o^");
            EXPECT_EQ((fastring() << cs << ss << x << yz << "^o^"), "csssxyz^o^");

            s.clear();
            s << s;
            EXPECT_EQ(s, "");
            s << "x";
            s << s;
            EXPECT_EQ(s, "xx");
            s.clear();
        }

        {
            int x = 0;
            s << &x;
            EXPECT_NE(s, "");
            EXPECT_EQ(s.substr(0, 2), "0x");
            s.clear();
        }
    }

    DEF_case(substr) {
        fastring s = "helloworld";
        EXPECT_EQ(s.substr(0), s);
        EXPECT_EQ(s.substr(0, 2), "he");
        EXPECT_EQ(s.substr(5), "world");
        EXPECT_EQ(s.substr(5, 2), "wo");
        EXPECT_EQ(s.substr(88), "");
        EXPECT_EQ(s.substr(88, 2), "");
    }

    DEF_case(compare) {
        fastring fs;
        std::string ss;
        const char* cs = "";
        EXPECT_EQ(fs.compare(ss), 0);
        EXPECT_EQ(fs.compare(cs), 0);

        fastring s = "88888888";
        EXPECT_EQ("88888888", s);
        EXPECT_EQ(s, "88888888");
        EXPECT_EQ(s, fastring("88888888"));
        EXPECT_EQ(s, std::string("88888888"));

        EXPECT_NE("8888888", s);
        EXPECT_NE(s, "8888888");
        EXPECT_NE(s, fastring("8888888"));
        EXPECT_NE(s, std::string("8888888"));
        EXPECT_NE(s, "888888888");
        EXPECT_NE(s, "xxxxxx");

        EXPECT_LT("7777777", s);
        EXPECT_GT(s, "7777777");
        EXPECT_GT(s, fastring("7777777"));
        EXPECT_GT(s, std::string("7777777"));
        EXPECT_GT(s, "77777777");
        EXPECT_GT(s, "777777777");

        EXPECT_GT("9999999", s);
        EXPECT_LT(s, "9999999");
        EXPECT_LT(s, fastring("9999999"));
        EXPECT_LT(s, std::string("9999999"));
        EXPECT_LT(s, "99999999");
        EXPECT_LT(s, "999999999");

        EXPECT_LE("88888888", s);
        EXPECT_GE(s, "88888888");
        EXPECT_GE(s, fastring("88888888"));
        EXPECT_GE(s, std::string("88888888"));
        EXPECT_GE(s, "777777777")

        EXPECT_GE("88888888", s);
        EXPECT_LE(s, "88888888");
        EXPECT_LE(s, fastring("88888888"));
        EXPECT_LE(s, std::string("88888888"));
        EXPECT_LE(s, "9999999");

        EXPECT_LT(s, "888888880");
        EXPECT_GT(s, "8888888");

        EXPECT_EQ(s.compare(3, 3, "888"), 0);
        EXPECT_EQ(s.compare(100, 3, ""), 0);
        EXPECT_EQ(s.compare(5, 100, "888"), 0);
        EXPECT_LT(s.compare(5, 3, "99"), 0);
        EXPECT_GT(s.compare(5, 3, "7777"), 0);

        fastring x("777888");
        EXPECT_EQ(s.compare(3, 3, x, 3), 0);
        EXPECT_EQ(s.compare(3, 3, x, 3, 8), 0);
        EXPECT_GT(s.compare(3, 3, x, 1, 4), 0);
        EXPECT_LT(s.compare(3, 2, x, 3, 3), 0);
    }

    DEF_case(find) {
        fastring s("xxxyyyzzz");
        EXPECT_EQ(s.find('a'), s.npos);
        EXPECT_EQ(s.find('y'), 3);
        EXPECT_EQ(s.find('Y'), s.npos);
        EXPECT_EQ(s.ifind('Y'), 3);
        EXPECT_EQ(s.ifind('Y', 5), 5);
        EXPECT_EQ(s.find('y', 0, 3), s.npos); // find in range [0, 3)
        EXPECT_EQ(s.find('y', 0, 4), 3);      // find in range [0, 4)
        EXPECT_EQ(s.find('y', 3, 3), 3);      // find in range [3, 6)
        EXPECT_EQ(s.find('y', 4, 3), 4);      // find in range [4, 7)
        EXPECT_EQ(s.find('y', 6, 3), s.npos); // find in range [6, 9)
        EXPECT_EQ(s.find('y', 4), 4);
        EXPECT_EQ(s.find('y', 32), s.npos);
        EXPECT_EQ(s.rfind('a'), s.npos);
        EXPECT_EQ(s.rfind('x'), 2);
        EXPECT_EQ(s.rfind('y'), 5);
        EXPECT_EQ(s.rfind('y', 3), 3);
        EXPECT_EQ(s.rfind('y', 2), s.npos);
        EXPECT_EQ(s.rfind('z'), 8);
        EXPECT_EQ(s.rfind("xyz"), s.npos);
        EXPECT_EQ(s.rfind("xy"), 2);
        EXPECT_EQ(s.rfind("yy"), 4);
        EXPECT_EQ(s.rfind("yy", 32), 4);
        EXPECT_EQ(s.rfind("yy", 4), 3);
        EXPECT_EQ(fastring("0123456789").rfind("0"), 0);
        EXPECT_EQ(fastring("0123456789").rfind("01"), 0);
        EXPECT_EQ(fastring("0123456789").rfind("012"), 0);
        EXPECT_EQ(fastring("0123456789").rfind("0123456789"), 0);
        EXPECT_EQ(fastring("0123456789").rfind("01234567890"), s.npos);
        EXPECT_EQ(fastring("0123456789").rfind("1234"), 1);
        EXPECT_EQ(fastring("0123456789").rfind("23456"), 2);
        EXPECT_EQ(fastring("0123456789").rfind("345678"), 3);
        EXPECT_EQ(fastring("0123456789").rfind("56789"), 5);
        EXPECT_EQ(fastring("0123456789").rfind("0124"), s.npos);
        EXPECT_EQ(fastring("0123\0abcd", 9).rfind("abc"), 5);

        EXPECT_EQ(s.find("xy"), 2);
        EXPECT_EQ(s.find("yy"), 3);
        EXPECT_EQ(s.find("yy", 4), 4);
        EXPECT_EQ(s.find("XY"), s.npos);
        EXPECT_EQ(s.ifind("XY"), 2);
        EXPECT_EQ(s.ifind("YY", 4), 4);
        EXPECT_EQ(s.ifind("Yy", 4), 4);
        EXPECT_EQ(fastring().ifind("xx"), s.npos);

        EXPECT_EQ(s.find_first_of("xy"), 0);
        EXPECT_EQ(s.find_first_of("yz"), 3);
        EXPECT_EQ(s.find_first_of("xy", 2), 2);
        EXPECT_EQ(s.find_first_of("yz", 2), 3);
        EXPECT_EQ(s.find_first_not_of('x'), 3);
        EXPECT_EQ(s.find_first_not_of('x', 2), 3);
        EXPECT_EQ(s.find_first_not_of('x', 4), 4);
        EXPECT_EQ(s.find_first_not_of("xy"), 6);
        EXPECT_EQ(s.find_first_not_of("xy", 2), 6);
        EXPECT_EQ(s.find_first_not_of("xy", 7), 7);

        s = "xyz";
        EXPECT_EQ(s.find_last_of("ax"), 0);
        EXPECT_EQ(s.find_last_of("ax", 1), 0);
        EXPECT_EQ(s.find_last_of("ax", 3), 0);
        EXPECT_EQ(s.find_last_of("xy"), 1);
        EXPECT_EQ(s.find_last_of("yz"), 2);
        EXPECT_EQ(s.find_last_of("yz", 0), s.npos);
        EXPECT_EQ(s.find_last_of("xz"), 2);
        EXPECT_EQ(s.find_last_of("abc"), s.npos);
        EXPECT_EQ(s.find_last_not_of('x'), 2);
        EXPECT_EQ(s.find_last_not_of('x', 1), 1);
        EXPECT_EQ(s.find_last_not_of('z'), 1);
        EXPECT_EQ(s.find_last_not_of("yz"), 0);
        EXPECT_EQ(s.find_last_not_of("yz", 1), 0);
        EXPECT_EQ(s.find_last_not_of("xyz"), s.npos);
    }

    DEF_case(replace) {
        fastring s("1122332211");
        EXPECT_EQ(s.replace("22", "xx"), "11xx33xx11");
        EXPECT_EQ(s.replace("xx", "22", 1), "112233xx11");

        s = "xxxxx";
        EXPECT_EQ(s.replace("xx", "yy"), "yyyyx");

        s = "xxxxx";
        EXPECT_EQ(s.replace("xxx", "x"), "xxx");

        s = "xxxxxxxxx";
        EXPECT_EQ(s.replace("xxx", "x"), "xxx");
        EXPECT_EQ(s.replace("x", "xx"), "xxxxxx");
    }

    DEF_case(trim) {
        fastring s("xx1122332211");
        const char* p = s.data();

        EXPECT_EQ(s.trim("x", 'l'), "1122332211");
        EXPECT_EQ(s.data(), p);

        EXPECT_EQ(s.trim("1", 'r'), "11223322");
        EXPECT_EQ(s.data(), p);

        EXPECT_EQ(s.trim("12"), "33");
        EXPECT_EQ(s.data(), p);

        EXPECT_EQ(s.trim("3"), "");
        EXPECT_EQ(s.data(), p);

        s = "xxxyyy";
        EXPECT_EQ(s.trim("xy"), "");

        s = "xxx";
        EXPECT_EQ(s.trim("xy", 'r'), "");

        s = "xxx";
        EXPECT_EQ(s.trim("xy", 'l'), "");

        fastring x("123456789");
        x.trim(1);
        EXPECT_EQ(x, "2345678");
        x.trim(2, 'l');
        EXPECT_EQ(x, "45678");
        x.trim(2, 'r');
        EXPECT_EQ(x, "456");

        x.trim(5, 'r');
        EXPECT(x.empty());

        x = "123456";
        x.trim(7, 'l');
        EXPECT(x.empty());

        x = "123456";
        x.trim(4);
        EXPECT(x.empty());

        EXPECT_EQ(fastring("hello").trim(0, 'l'), "hello");
        EXPECT_EQ(fastring("hello").trim(1, 'l'), "ello");
        EXPECT_EQ(fastring("hello").trim(2, 'l'), "llo");
        EXPECT_EQ(fastring("hello").trim(3, 'l'), "lo");
        EXPECT_EQ(fastring("hello").trim(4, 'l'), "o");
        EXPECT_EQ(fastring("hello").trim(5, 'l'), "");
        EXPECT_EQ(fastring("hello").trim(6, 'l'), "");
    }

    DEF_case(starts_ends) {
        fastring s("xxxyyyzzz");
        EXPECT(s.starts_with('x'));
        EXPECT(s.starts_with("xx"));
        EXPECT(s.starts_with("xxx"));
        EXPECT(s.starts_with("xxxy"));
        EXPECT(s.starts_with("xxxyyy"));
        EXPECT(s.starts_with("xxxyyy", 6));
        EXPECT(s.ends_with('z'));
        EXPECT(s.ends_with("zzz"));
        EXPECT(s.ends_with("zzz", 3));
        EXPECT(s.ends_with("yyzzz", 5));
    }

    DEF_case(remove_suffix) {
        fastring s("xx.log");
        EXPECT_EQ(s.remove_suffix(".log"), "xx");
        EXPECT_EQ(s.remove_suffix(".log"), "xx");

        s = "xx.exe";
        fastring x(".exe");
        EXPECT_EQ(s.remove_suffix(x), "xx");

        s = "xx.exe";
        std::string e(".exe");
        EXPECT_EQ(s.remove_suffix(e), "xx");
    }

    DEF_case(remove_prefix) {
        fastring s("xxhello");
        EXPECT_EQ(s.remove_prefix("xx"), "hello");
        EXPECT_EQ(s.remove_prefix("xx"), "hello");
        EXPECT_EQ(s.remove_prefix("hex"), "hello");
    }

    DEF_case(upperlower) {
        fastring s("xYz88");
        EXPECT_EQ(s.upper(), "XYZ88");
        EXPECT_EQ(s.lower(), "xyz88");
        EXPECT_EQ(s, "xYz88");

        s.tolower();
        EXPECT_EQ(s, "xyz88");

        s.toupper();
        EXPECT_EQ(s, "XYZ88");
    }

    DEF_case(match) {
        fastring s("hello");
        EXPECT(s.match("*"));
        EXPECT(s.match("*hello"));
        EXPECT(s.match("*ello"));
        EXPECT(s.match("h*"));
        EXPECT(s.match("h*o"));
        EXPECT(s.match("he??o"));
        EXPECT(s.match("h?llo"));
        EXPECT(s.match("h?l*"));
        EXPECT(s.match("h*l?o"));
        EXPECT(s.match("h*l*o"));
        EXPECT(s.match("*ll*"));
        EXPECT(s.match("h**l**o"));
        EXPECT(!s.match("h?o"));
    }

    DEF_case(clear) {
        fastring s("xxx");
        s.clear(0);
        EXPECT_EQ(s.size(), 0);
        s.resize(3);
        EXPECT_EQ(s[0], 0);
        EXPECT_EQ(s[1], 0);
        EXPECT_EQ(s[2], 0);

        fastring x;
        x.clear(0);
        EXPECT_EQ(x.size(), 0);
    }

    DEF_case(shrink) {
        fastring s(256);
        (s = "hello world").shrink();
        EXPECT_LT(s.capacity(), 256);
        EXPECT_EQ(s.capacity(), 12);
        EXPECT_EQ(s, "hello world");
    }

    DEF_case(plus) {
        fastring a("hello");
        fastring b("world");
        const char* s = "again";
        char c = 'x';
        EXPECT_EQ(a + b, "helloworld");
        EXPECT_EQ(b + a, "worldhello");
        EXPECT_EQ(a + s, "helloagain");
        EXPECT_EQ(s + a, "againhello");
        EXPECT_EQ(a + c, "hellox");
        EXPECT_EQ(c + a, "xhello");
    }
}

} // namespace test
