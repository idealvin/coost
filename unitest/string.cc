#include "co/unitest.h"
#include "co/string.h"

namespace test {

DEF_test(string) {
    DEF_case(constructor) {
        {
            co::string s;
            EXPECT(s.empty());
            EXPECT_EQ(s.size(), 0);
            EXPECT_EQ(s.capacity(), 0);
        }
        {
            co::string s(32);
            EXPECT(s.empty());
            EXPECT_EQ(s.capacity(), 32);
        }
        {
            co::string s(4, 'x');
            EXPECT_EQ(s.size(), 4);
            EXPECT_EQ(s.capacity(), 5);
            EXPECT_EQ(s, "xxxx");
        }
        {
            co::string s(nullptr, 0);
            EXPECT(s.empty());
            EXPECT_EQ(s.capacity(), 0);
        }
        {
            co::string s("helloworld", 5);
            EXPECT_EQ(s.size(), 5);
            EXPECT_EQ(s, "hello");
            EXPECT_LT(s.size(), s.capacity());
        }
        {
            co::string s("helloworld");
            EXPECT_EQ(s.size(), 10);
            EXPECT_EQ(s, "helloworld");
            EXPECT_LT(s.size(), s.capacity());

            co::string x(nullptr);
            EXPECT_EQ(x.size(), 0);
        }
        {
            co::string x("hello");
            co::string s(x);
            EXPECT_EQ(s.size(), 5);
            EXPECT_EQ(s, "hello");
            EXPECT_LT(s.size(), s.capacity());
        }
        {
            std::string x("hello");
            co::string s(x);
            EXPECT_EQ(s.size(), 5);
            EXPECT_EQ(s, "hello");
            EXPECT_LT(s.size(), s.capacity());
        }
        {
            co::string x("hello");
            void* p = x.data();

            co::string s(std::move(x));
            EXPECT_EQ(s.size(), 5);
            EXPECT_EQ((void*)s.data(), p);
            EXPECT_EQ(s, "hello");
            EXPECT_LT(s.size(), s.capacity());
            EXPECT_EQ(x.size(), 0);
            EXPECT_EQ(x.capacity(), 0);
            EXPECT_EQ((size_t)x.data(), 0)
        }
    }

    DEF_case(operator=) {
        co::string x("abc");
        co::string s("123");
        {
            s = std::move(x);
            EXPECT_EQ(s, "abc")
            EXPECT(x.empty())
        }
        {
            s = "12345";
            EXPECT_EQ(s, "12345")
            EXPECT_LT(s.size(), s.capacity());

            s = s.c_str() + 2;
            EXPECT_EQ(s, "345")
        }
        {
            size_t p = (size_t)s.data();
            s = s;
            EXPECT_EQ((size_t)s.data(), p)

            x = "xxx";
            s = x;
            EXPECT_EQ(s, "xxx")
        }
        {
            std::string y("yyy");
            s = y;
            EXPECT_EQ(s, "yyy")
        }
    }

    DEF_case(assign) {
        co::string s("hello");
        {
            s.assign("12345", 5);
            EXPECT_EQ(s, "12345");

            s.assign(s.data() + 1, 4);
            EXPECT_EQ(s, "2345");
        }
        {
            s.assign(3, '0');
            EXPECT_EQ(s, "000");

            s.assign(31, '0');
            EXPECT_EQ(s, co::string(31, '0'));
        }
        {
            s.assign("12345");
            EXPECT_EQ(s, "12345");

            s = s.c_str() + 2;
            EXPECT_EQ(s, "345");
        }
        {
            co::string x("xxx");
            s.assign(x);
            EXPECT_EQ(s, "xxx")
            EXPECT_EQ(x, "xxx")

            s.assign(std::move(x));
            EXPECT_EQ(s, "xxx")
            EXPECT(x.empty())
        }
        {
            std::string x("ddd");
            s.assign(x);
            EXPECT_EQ(s, "ddd")
        }
    }

    DEF_case(clear|zero_clear) {
        co::string s("hello");
        {
            size_t cap = s.capacity();
            char* p = s.data();

            s.clear();
            EXPECT(s.empty())
            EXPECT_EQ(s.capacity(), cap)

            EXPECT_EQ(p[0], 'h')
            EXPECT_EQ(p[1], 'e')
            EXPECT_EQ(p[4], 'o')
        }
        {
            s = "hello";
            char* p = s.data();

            s.zero_clear();
            EXPECT(s.empty())
            EXPECT_EQ((size_t)s.data(), (size_t)p)
            EXPECT_EQ(p[0], 0)
            EXPECT_EQ(p[1], 0)
            EXPECT_EQ(p[4], 0)
        }
    }

    DEF_case(c_str) {
        {
            co::string s;
            EXPECT_EQ(co::string(s.c_str()), "");
        }
        {
            co::string s(32, 'x');
            size_t cap = s.capacity();
            const char* p = s.data();
            const char* q = s.c_str();
            EXPECT_EQ(p, q)
            EXPECT_EQ(p[32], '\0')
            EXPECT_EQ(s.capacity(), cap)
        }
    }

    DEF_case(back|front|operator[]) {
        co::string s("hello");
        EXPECT_EQ(s.front(), 'h')
        EXPECT_EQ(s.back(), 'o')

        s.front() = 'g';
        EXPECT_EQ(s.front(), 'g')

        s.back() = 'x';
        EXPECT_EQ(s.back(), 'x')

        EXPECT_EQ(s[1], 'e')
        EXPECT_EQ(s[2], 'l')

        s[3] = 'k';
        EXPECT_EQ(s[3], 'k')
    }

    DEF_case(resize|reserve|ensure|reset) {
        co::string s("helloworld");
        {
            size_t cap = s.capacity();
            s.resize(5);
            EXPECT_EQ(s.size(), 5)
            EXPECT_EQ(s, "hello")
            EXPECT_EQ(s.capacity(), cap)

            s.resize(10);
            EXPECT_EQ(s.size(), 10)
            EXPECT_EQ(s, "helloworld")
            EXPECT_EQ(s.capacity(), cap)

            s.resize(12);
            EXPECT_EQ(s.size(), 12)
            EXPECT_LT(s.size(), s.capacity())

            s.resize(23);
            EXPECT_EQ(s.substr(0, 10), "helloworld")
            EXPECT_EQ(s.size(), 23)
            EXPECT_LT(s.size(), s.capacity())
        }
        {
            size_t cap = s.capacity();
            s.reserve(3);
            EXPECT_EQ(s.capacity(), cap)

            s.reserve(32);
            EXPECT_EQ(s.capacity(), 32)
        }
        {
            s.resize(10);
            s.ensure(21);
            EXPECT_EQ(s.capacity(), 32)

            s.ensure(22);
            EXPECT_GT(s.capacity(), 32)
        }
        {
            s.reset();
            EXPECT_EQ((size_t)s.data(), 0)
            EXPECT_EQ(s.size(), 0)
            EXPECT_EQ(s.capacity(), 0)
        }
    }

    DEF_case(swap) {
        co::string x("xxx");
        co::string y("yyy");
        x.swap(y);
        EXPECT_EQ(x, "yyy")
        EXPECT_EQ(y, "xxx")

        x.swap(std::move(y));
        EXPECT_EQ(x, "xxx")
        EXPECT_EQ(y, "yyy")
    }

    DEF_case(append|append_nomchk|operator+=) {
        co::string s;
        {
            s.append('x');
            EXPECT_EQ(s, "x");
        }
        {
            s.append(2, 'x');
            EXPECT_EQ(s.size(), 3);
            EXPECT_EQ(s, "xxx");
            EXPECT_LT(s.size(), s.capacity());
        }
        {
            s.append("yyy", 3);
            EXPECT_EQ(s.size(), 6);
            EXPECT_EQ(s, "xxxyyy");
            EXPECT_LT(s.size(), s.capacity());

            s.append(s.c_str() + 1, 4);
            EXPECT_EQ(s, "xxxyyyxxyy");
            EXPECT_LT(s.size(), s.capacity());

            co::string _("ddddd");
            EXPECT(!_.empty());

            size_t p = (size_t)s.data();
            size_t cap = s.capacity();
            s.append(s.c_str() + 1, 9);
            EXPECT_EQ(s.size(), 19)
            EXPECT_GT(s.capacity(), cap)
            EXPECT_NE((size_t)s.data(), p)
        }
        {
            s.resize(3);
            s.append("yyy");
            EXPECT_EQ(s, "xxxyyy");

            s.append(s.c_str() + 2);
            EXPECT_EQ(s, "xxxyyyxyyy");
        }
        {
            co::string s("xxx");
            s.append(s);
            EXPECT_EQ(s, "xxxxxx");
            EXPECT_LT(s.size(), s.capacity());

            s.append(s);
            co::string _("ddddd");
            EXPECT(!_.empty())

            size_t p = (size_t)s.data();
            size_t cap = s.capacity();
            s.append(s);
            EXPECT_EQ(s.size(), 24)
            EXPECT_NE((size_t)s.data(), p)
            EXPECT_GT(s.capacity(), cap)
        }
        {
            co::string x("xxx");
            std::string y("yyy");
            x.append(y);
            EXPECT_EQ(x, "xxxyyy")
            EXPECT_LT(s.size(), s.capacity());
        }
        {
            co::string s("xxx");
            s.append_nomchk("yyy", 3);
            EXPECT_EQ(s, "xxxyyy")
            EXPECT_LT(s.size(), s.capacity());

            s.append_nomchk("zzz");
            EXPECT_EQ(s, "xxxyyyzzz")
        }
        {
            co::string s("xxx");
            s += 'v';
            EXPECT_EQ(s, "xxxv")

            s += "yyy";
            EXPECT_EQ(s, "xxxvyyy")

            s += s;
            EXPECT_EQ(s.size(), 14);
        }
    }

    DEF_case(push_back|pop_back) {
        co::string s;
        {
            s.push_back('x');
            EXPECT_EQ(s.back(), 'x')
        }
        {
            EXPECT_EQ(s.pop_back(), 'x')
            EXPECT(s.empty());
        }
    }

    DEF_case(operator<<) {
        co::string s;
        {
            s << false << ' ' << true;
            EXPECT_EQ(s, "false true");
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
            EXPECT_EQ(s, "0.5");
            s.clear();

            s << d;
            EXPECT_EQ(s, "3.14159");
            s.clear();

            s << co::decimal(d, 2);
            EXPECT_EQ(s, "3.14");
            s.clear();
        }
        {
            int x = 0;
            s << &x;
            EXPECT_EQ(s.substr(0, 2), "0x");
            s.clear();

            union {
                void* p;
                size_t u;
            };
            u = 0x12345678;
            s << p;
            EXPECT_EQ(s, "0x12345678")
            s.clear();

            s << nullptr;
            EXPECT_EQ(s, "0x0")
            s.clear();
        }
        {
            const char* cs = "cs";
            std::string ss = "ss";
            co::string x = "x";
            char yz[4];
            yz[0] = 'y';
            yz[1] = 'z';
            yz[2] = '\0';
            yz[3] = 'x';

            s << cs << ss << x << yz << "^o^";
            EXPECT_EQ(s, "csssxyz^o^");

            s.clear();
            s << s;
            EXPECT_EQ(s, "");
            s << "x";
            s << s;
            EXPECT_EQ(s, "xx");
            s.clear();
        }
        {
            co::string s;
            s << co::max_uint64;
            EXPECT_LT(s.size(), 24);

            s.clear();
            s << co::min_int64;
            EXPECT_LT(s.size(), 24);

            s.clear();
            s << (void*) co::max_uint64;
            EXPECT_LT(s.size(), 24);

            s.clear();
            s << 1234567890.0123456789876543210;
            EXPECT_LT(s.size(), 24);
        }
    }

    DEF_case(cat) {
        co::string s;
        EXPECT_EQ(s.cat(), "");
        EXPECT_EQ(s.cat(0.1, 2, 3), "0.123");
        EXPECT_EQ(s.cat(' ', "hello ", false), "0.123 hello false");
    }

    DEF_case(compare) {
        {
            co::string s;
            std::string x;
            const char* c = "";
            EXPECT_EQ(s.compare(x), 0);
            EXPECT_EQ(s.compare(c), 0);
        }

        co::string s = "88888888";
        {
            EXPECT_EQ("88888888", s);
            EXPECT_EQ(s, "88888888");
            EXPECT_EQ(s, co::string("88888888"));
            EXPECT_EQ(s, std::string("88888888"));
        }
        {
            EXPECT_NE("8888888", s);
            EXPECT_NE(s, "8888888");
            EXPECT_NE(s, co::string("8888888"));
            EXPECT_NE(s, std::string("8888888"));
            EXPECT_NE(s, "888888888");
            EXPECT_NE(s, "xxxxxx");
        }
        {
            EXPECT_LT("7777777", s);
            EXPECT_GT(s, "7777777");
            EXPECT_GT(s, co::string("7777777"));
            EXPECT_GT(s, std::string("7777777"));
            EXPECT_GT(s, "77777777");
            EXPECT_GT(s, "777777777");

            EXPECT_GT("9999999", s);
            EXPECT_LT(s, "9999999");
            EXPECT_LT(s, co::string("9999999"));
            EXPECT_LT(s, std::string("9999999"));
            EXPECT_LT(s, "99999999");
            EXPECT_LT(s, "999999999");

            EXPECT_LT(s, "888888880");
            EXPECT_GT(s, "8888888");
        }
        {
            EXPECT_LE("88888888", s);
            EXPECT_GE(s, "88888888");
            EXPECT_GE(s, co::string("88888888"));
            EXPECT_GE(s, std::string("88888888"));
            EXPECT_GE(s, "777777777")

            EXPECT_GE("88888888", s);
            EXPECT_LE(s, "88888888");
            EXPECT_LE(s, co::string("88888888"));
            EXPECT_LE(s, std::string("88888888"));
            EXPECT_LE(s, "9999999");
        }
    }

    DEF_case(contains) {
        co::string s("xxxyyyzzz");
        EXPECT(s.contains('y'))
        EXPECT(!s.contains('o'))
        EXPECT(s.contains("xyyyz"))
        EXPECT(!s.contains("xxyyyo"))
        EXPECT(s.contains(co::string("yyy")))
        EXPECT(s.contains(std::string("yyy")))
    }

    DEF_case(starts_with|ends_with) {
        co::string s("xxxyyyzzz");
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

    DEF_case(find) {
        co::string s("xxxyyyzzz");
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
        EXPECT_EQ(co::string("0123456789").rfind("0"), 0);
        EXPECT_EQ(co::string("0123456789").rfind("01"), 0);
        EXPECT_EQ(co::string("0123456789").rfind("012"), 0);
        EXPECT_EQ(co::string("0123456789").rfind("0123456789"), 0);
        EXPECT_EQ(co::string("0123456789").rfind("01234567890"), s.npos);
        EXPECT_EQ(co::string("0123456789").rfind("1234"), 1);
        EXPECT_EQ(co::string("0123456789").rfind("23456"), 2);
        EXPECT_EQ(co::string("0123456789").rfind("345678"), 3);
        EXPECT_EQ(co::string("0123456789").rfind("56789"), 5);
        EXPECT_EQ(co::string("0123456789").rfind("0124"), s.npos);
        EXPECT_EQ(co::string("0123\0abcd", 9).rfind("abc"), 5);

        EXPECT_EQ(s.find("xy"), 2);
        EXPECT_EQ(s.find("yy"), 3);
        EXPECT_EQ(s.find("yy", 4), 4);
        EXPECT_EQ(s.find("XY"), s.npos);
        EXPECT_EQ(s.ifind("XY"), 2);
        EXPECT_EQ(s.ifind("YY", 4), 4);
        EXPECT_EQ(s.ifind("Yy", 4), 4);
        EXPECT_EQ(co::string().ifind("xx"), s.npos);

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

    DEF_case(match) {
        co::string s("hello");
        EXPECT(s.match("hello"))
        EXPECT(s.match("*"))
        EXPECT(s.match("*lo"))
        EXPECT(s.match("he*"))
        EXPECT(s.match("h*o"))
        EXPECT(s.match("he??o"))
        EXPECT(s.match("h*??o"))
        EXPECT(!s.match("h?o"))
        EXPECT(!s.match("*x"))
        EXPECT(!s.match("x*"))
        EXPECT(!s.match("h*x*o"))
    }

    DEF_case(lower|upper|tolower|toupper) {
        co::string s("xYz88");
        EXPECT_EQ(s.upper(), "XYZ88");
        EXPECT_EQ(s.lower(), "xyz88");
        EXPECT_EQ(s, "xYz88");

        s.tolower();
        EXPECT_EQ(s, "xyz88");

        s.toupper();
        EXPECT_EQ(s, "XYZ88");
    }

    DEF_case(substr) {
        co::string s = "helloworld";
        EXPECT_EQ(s.substr(0), s);
        EXPECT_EQ(s.substr(0, 2), "he");
        EXPECT_EQ(s.substr(5), "world");
        EXPECT_EQ(s.substr(5, 2), "wo");
        EXPECT_EQ(s.substr(88), "");
        EXPECT_EQ(s.substr(88, 2), "");
    }

    DEF_case(escape|unescape) {
        {
            co::string s;
            s.escape();
            EXPECT(s.empty());
            s.unescape();
            EXPECT(s.empty());
        }
        {
            co::string s("\"\r\n\t\a\b\f\v");
            s.append('\0').append('\\');
            EXPECT_EQ(s.size(), 10)
            co::string x(s);

            s.escape();
            EXPECT_EQ(s.size(), 20)
            EXPECT_EQ(s, "\\\"\\r\\n\\t\\a\\b\\f\\v\\0\\\\")
            EXPECT_LT(s.size(), s.capacity())

            s.unescape();
            EXPECT_EQ(s.size(), 10)
            EXPECT_EQ(s, x)
        }
        {
            co::string s("a\\'b");
            s.unescape();
            EXPECT_EQ(s, "a'b")
            s.escape();
            EXPECT_EQ(s, "a'b")
        }
    }

    DEF_case(replace) {
        co::string s("1122332211");
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

    DEF_case(trim|trim_left|trim_right) {
        {
            co::string s("xabcx");
            s.trim('x');
            EXPECT_EQ(s, "abc")

            s.trim_left('a');
            EXPECT_EQ(s, "bc")

            s.trim_right('c');
            EXPECT_EQ(s, "b")
        }
        {
            co::string s("xx1122332211");
            const char* p = s.c_str();

            EXPECT_EQ(s.trim_left("x"), "1122332211");
            EXPECT(s.data() == p);

            EXPECT_EQ(s.trim_right("1"), "11223322");
            EXPECT(s.data() == p);

            EXPECT_EQ(s.trim("12"), "33");
            EXPECT(s.data() == p);

            EXPECT_EQ(s.trim("3"), "");
            EXPECT(s.data() == p);

            s = "xxxyyy";
            EXPECT_EQ(s.trim("xy"), "");

            s = "xxx";
            EXPECT_EQ(s.trim_right("xy"), "");

            s = "xxx";
            EXPECT_EQ(s.trim_left("xy"), "");
        }
    }

    DEF_case(remove_prefix|remove_suffix) {
        {
            co::string s("xxhello");
            EXPECT_EQ(s.remove_prefix("xx"), "hello");
            EXPECT_EQ(s.remove_prefix("xx"), "hello");
            EXPECT_EQ(s.remove_prefix("hex"), "hello");
        }
        {
            co::string s("xx.log");
            EXPECT_EQ(s.remove_suffix(".log"), "xx");
            EXPECT_EQ(s.remove_suffix(".log"), "xx");

            s = "xx.exe";
            co::string x(".exe");
            EXPECT_EQ(s.remove_suffix(x), "xx");

            s = "xx.exe";
            std::string e(".exe");
            EXPECT_EQ(s.remove_suffix(e), "xx");
        }
    }

    DEF_case(remove_outer|remove_prefix|remove_suffix) {
        co::string x("123456789");
        x.remove_outer(1);
        EXPECT_EQ(x, "2345678");

        x.remove_prefix(2);
        EXPECT_EQ(x, "45678");

        x.remove_suffix(2);
        EXPECT_EQ(x, "456");

        x.remove_suffix(5);
        EXPECT(x.empty());

        x = "123456";
        x.remove_prefix(7);
        EXPECT(x.empty());

        x = "123456";
        x.remove_outer(4);
        EXPECT(x.empty());
    }

    DEF_case(shrink_to_fit) {
        co::string s(256);
        (s = "hello world").shrink_to_fit();
        EXPECT_LT(s.capacity(), 256);
        EXPECT_EQ(s.capacity(), 12);
        EXPECT_EQ(s, "hello world");
    }

    DEF_case(operator+) {
        co::string a("hello");
        co::string b("world");
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

DEF_test(strconv) {
    char buf[32];

    DEF_case(itoa) {
        EXPECT_EQ(co::string(buf, co::itoa(0, buf, 24)), "0");
        EXPECT_EQ(co::string(buf, co::itoa(1, buf, 24)), "1");
        EXPECT_EQ(co::string(buf, co::itoa(12, buf, 24)), "12");
        EXPECT_EQ(co::string(buf, co::itoa(123, buf, 24)), "123");
        EXPECT_EQ(co::string(buf, co::itoa(1234, buf, 24)), "1234");
        EXPECT_EQ(co::string(buf, co::itoa(12345, buf, 24)), "12345");
        EXPECT_EQ(co::string(buf, co::itoa(123456, buf, 24)), "123456");
        EXPECT_EQ(co::string(buf, co::itoa(1234567, buf, 24)), "1234567");
        EXPECT_EQ(co::string(buf, co::itoa(12345678, buf, 24)), "12345678");
        EXPECT_EQ(co::string(buf, co::itoa(123456789, buf, 24)), "123456789");
        EXPECT_EQ(co::string(buf, co::itoa(1234567890, buf, 24)), "1234567890");
        EXPECT_EQ(co::string(buf, co::itoa(3234567890U, buf, 24)), "3234567890");

        EXPECT_EQ(co::string(buf, co::itoa(123456789012ULL, buf, 24)), "123456789012");
        EXPECT_EQ(co::string(buf, co::itoa(12345678901234ULL, buf, 24)), "12345678901234");
        EXPECT_EQ(co::string(buf, co::itoa(123456789012345ULL, buf, 24)), "123456789012345");
        EXPECT_EQ(co::string(buf, co::itoa(1234567890123456ULL, buf, 24)), "1234567890123456");
        EXPECT_EQ(co::string(buf, co::itoa(12345678901234567ULL, buf, 24)), "12345678901234567");
        EXPECT_EQ(co::string(buf, co::itoa(123456789012345678ULL, buf, 24)), "123456789012345678");
        EXPECT_EQ(co::string(buf, co::itoa(1234567890123456789ULL, buf, 24)), "1234567890123456789");
        EXPECT_EQ(co::string(buf, co::itoa(12345678901234567890ULL, buf, 24)), "12345678901234567890");

        EXPECT_EQ(co::string(buf, co::itoa(0, buf, 24)), "0");
        EXPECT_EQ(co::string(buf, co::itoa(1, buf, 24)), "1");
        EXPECT_EQ(co::string(buf, co::itoa(-9, buf, 24)), "-9");
        EXPECT_EQ(co::string(buf, co::itoa(1234567, buf, 24)), "1234567");
        EXPECT_EQ(co::string(buf, co::itoa(-12345678, buf, 24)), "-12345678");
        EXPECT_EQ(co::string(buf, co::itoa(-123456789, buf, 24)), "-123456789");

        EXPECT_EQ(co::string(buf, co::itoa(-9, buf, 24)), "-9");
        EXPECT_EQ(co::string(buf, co::itoa(-12345678, buf, 24)), "-12345678");
        EXPECT_EQ(co::string(buf, co::itoa(-123456789, buf, 24)), "-123456789");
        EXPECT_EQ(co::string(buf, co::itoa(-123456789012345LL, buf, 24)), "-123456789012345");
        EXPECT_EQ(co::string(buf, co::itoa(-123456789012345678LL, buf, 24)), "-123456789012345678");
        EXPECT_EQ(co::string(buf, co::itoa(-1234567890123456789LL, buf, 24)), "-1234567890123456789");
    }

    DEF_case(utoh) {
        EXPECT_EQ(co::string(buf, co::utoh(0u, buf, 24)), "0x0");
        EXPECT_EQ(co::string(buf, co::utoh(1u, buf, 24)), "0x1");
        EXPECT_EQ(co::string(buf, co::utoh(0xau, buf, 24)), "0xa");
        EXPECT_EQ(co::string(buf, co::utoh(0xfu, buf, 24)), "0xf");
        EXPECT_EQ(co::string(buf, co::utoh(0xffu, buf, 24)), "0xff");
        EXPECT_EQ(co::string(buf, co::utoh(0x123456u, buf, 24)), "0x123456");
        EXPECT_EQ(co::string(buf, co::utoh(0x1234567u, buf, 24)), "0x1234567");
        EXPECT_EQ(co::string(buf, co::utoh(0x12345678u, buf, 24)), "0x12345678");
        EXPECT_EQ(co::string(buf, co::utoh(0xffffffffu, buf, 24)), "0xffffffff");
        EXPECT_EQ(co::string(buf, co::utoh(0xfffffffffULL, buf, 24)), "0xfffffffff");
        EXPECT_EQ(co::string(buf, co::utoh(0xffffffffffffULL, buf, 24)), "0xffffffffffff");
        EXPECT_EQ(co::string(buf, co::utoh(0xffffffffffffffffULL, buf, 24)), "0xffffffffffffffff");
        EXPECT_EQ(co::string(buf, co::utoh(0x1234567890ULL, buf, 24)), "0x1234567890");
        EXPECT_EQ(co::string(buf, co::utoh(0x1234567890abcdefULL, buf, 24)), "0x1234567890abcdef");
    }

    DEF_case(ptoh) {
        EXPECT_EQ(co::string(buf, co::ptoh((void*)0x123456, buf, 24)), "0x123456");
        EXPECT_EQ(co::string(buf, co::ptoh((void*)0x12345678, buf, 24)), "0x12345678");
        if (sizeof(void*) == sizeof(uint64)) {
            EXPECT_EQ(co::string(buf, co::ptoh((void*)(size_t)0x1234567890abcdefULL, buf, 24)), "0x1234567890abcdef");
        }
    }

    DEF_case(dtoa) {
        EXPECT_EQ(co::string(buf, co::dtoa(0.0, buf)), "0.0");
        EXPECT_EQ(co::string(buf, co::dtoa(0.00, buf)), "0.0");
        EXPECT_EQ(co::string(buf, co::dtoa(0.01, buf)), "0.01");
        EXPECT_EQ(co::string(buf, co::dtoa(-0.1, buf)), "-0.1");
        EXPECT_EQ(co::string(buf, co::dtoa(3.14, buf)), "3.14");
        EXPECT_EQ(co::string(buf, co::dtoa(3.14159, buf)), "3.14159");
        EXPECT_EQ(co::string(buf, co::dtoa(3e-23, buf)), "3e-23");
        EXPECT_EQ(co::string(buf, co::dtoa(3.33e-23, buf)), "3.33e-23");
        EXPECT_EQ(co::string(buf, co::dtoa(3e23, buf)), "3e23");
        EXPECT_EQ(co::string(buf, co::dtoa(1e30, buf)), "1e30");
        EXPECT_EQ(co::string(buf, co::dtoa(3.33e23, buf)), "3.33e23");
        EXPECT_EQ(co::string(buf, co::dtoa(1.79e308, buf)), "1.79e308");
        EXPECT_EQ(co::string(buf, co::dtoa(2.23e-308, buf)), "2.23e-308");
        EXPECT_EQ(co::string(buf, co::dtoa(5e-324, buf)), "5e-324");
        EXPECT_EQ(co::string(buf, co::dtoa(0.001, buf)), "0.001");
        EXPECT_EQ(co::string(buf, co::dtoa(0.00123456, buf)), "0.00123456");
        EXPECT_EQ(co::string(buf, co::dtoa(0.0001, buf)), "1e-4");
        EXPECT_EQ(co::string(buf, co::dtoa(0.000001, buf)), "1e-6");
        EXPECT_EQ(co::string(buf, co::dtoa(0.0000001, buf)), "1e-7");
        EXPECT_EQ(co::string(buf, co::dtoa(0.123456789012, buf)), "0.123456789012");
        EXPECT_EQ(co::string(buf, co::dtoa(-79.39773355813419, buf)), "-79.39773355813419");
        EXPECT_EQ(co::string(buf, co::dtoa(1.234567890123456e30, buf)), "1.234567890123456e30");
        EXPECT_EQ(co::string(buf, co::dtoa(2.225073858507201e-308, buf)), "2.225073858507201e-308");
        EXPECT_EQ(co::string(buf, co::dtoa(2.2250738585072014e-308, buf)), "2.2250738585072014e-308");
        EXPECT_EQ(co::string(buf, co::dtoa(1.7976931348623157e308, buf)), "1.7976931348623157e308");
        EXPECT_EQ(co::string(buf, co::dtoa(-1.7976931348623157e308, buf)), "-1.7976931348623157e308");
        EXPECT_EQ(co::string(buf, co::dtoa(-1.7976931348623155e-308, buf)), "-1.7976931348623155e-308");

        double a = -1.7976931348623157e-308;
        double b = -1.7976931348623155e-308;
        EXPECT(memcmp(&a, &b, 8) == 0);

        EXPECT_EQ(co::string(buf, co::dtoa(0.123456, buf, 6)), "0.123456");
        EXPECT_EQ(co::string(buf, co::dtoa(0.123456, buf, 3)), "0.123");
        EXPECT_EQ(co::string(buf, co::dtoa(-0.123456, buf, 6)), "-0.123456");
        EXPECT_EQ(co::string(buf, co::dtoa(-0.123456, buf, 3)), "-0.123");
        EXPECT_EQ(co::string(buf, co::dtoa(0.0123456, buf, 7)), "0.0123456");
        EXPECT_EQ(co::string(buf, co::dtoa(0.0123456, buf, 6)), "1.23456e-2");
        EXPECT_EQ(co::string(buf, co::dtoa(0.0123456, buf, 3)), "1.234e-2");
        EXPECT_EQ(co::string(buf, co::dtoa(-0.0123456, buf, 7)), "-0.0123456");
        EXPECT_EQ(co::string(buf, co::dtoa(-0.0123456, buf, 3)), "-1.234e-2");
        EXPECT_EQ(co::string(buf, co::dtoa(0.00123456, buf, 8)), "0.00123456");
        EXPECT_EQ(co::string(buf, co::dtoa(0.00123456, buf, 3)), "1.234e-3");
        EXPECT_EQ(co::string(buf, co::dtoa(0.000123456, buf, 9)), "1.23456e-4");
        EXPECT_EQ(co::string(buf, co::dtoa(0.000123456, buf, 3)), "1.234e-4");
        EXPECT_EQ(co::string(buf, co::dtoa(-0.0000123, buf, 8)), "-1.23e-5");
        EXPECT_EQ(co::string(buf, co::dtoa(-0.0000123, buf, 3)), "-1.23e-5");
        EXPECT_EQ(co::string(buf, co::dtoa(1.234567, buf, 6)), "1.234567");
        EXPECT_EQ(co::string(buf, co::dtoa(1.234567, buf, 3)), "1.234");
        EXPECT_EQ(co::string(buf, co::dtoa(0.1000234567, buf, 10)), "0.1000234567");
        EXPECT_EQ(co::string(buf, co::dtoa(0.1000234567, buf, 5)), "0.10002");
        EXPECT_EQ(co::string(buf, co::dtoa(0.1000234567, buf, 3)), "0.1");
        EXPECT_EQ(co::string(buf, co::dtoa(1.1000234567, buf, 10)), "1.1000234567");
        EXPECT_EQ(co::string(buf, co::dtoa(1.1000234567, buf, 3)), "1.1");
        EXPECT_EQ(co::string(buf, co::dtoa(12345e3, buf, 6)), "12345000.0");
        EXPECT_EQ(co::string(buf, co::dtoa(123456789012345678901234.0, buf, 6)), "1.234567e23");
        EXPECT_EQ(co::string(buf, co::dtoa(1.23456789e-9, buf, 5)), "1.23456e-9");
        EXPECT_EQ(co::string(buf, co::dtoa(1.23456789e-9, buf, 2)), "1.23e-9");
        EXPECT_EQ(co::string(buf, co::dtoa(123000e30, buf, 2)), "1.23e35");
        EXPECT_EQ(co::string(buf, co::dtoa(123000e30, buf, 1)), "1.2e35");
        EXPECT_EQ(co::string(buf, co::dtoa(12345e-8, buf, 4)), "1.2345e-4");
        EXPECT_EQ(co::string(buf, co::dtoa(12345e-8, buf, 2)), "1.23e-4");
    }

    DEF_case(stox) {
        EXPECT_EQ(co::stob("true"), true);
        EXPECT_EQ(co::stob("1"), true);
        EXPECT_EQ(co::stob("false"), false);
        EXPECT_EQ(co::stob("0"), false);

        EXPECT_EQ(co::stoi32("-32"), -32);
        EXPECT_EQ(co::stoi32("-4k"), -4096);
        EXPECT_EQ(co::stoi64("-64"), -64);
        EXPECT_EQ(co::stoi64("-8G"), -(8LL << 30));

        EXPECT_EQ(co::stou32("32"), 32);
        EXPECT_EQ(co::stou32("4K"), 4096);
        EXPECT_EQ(co::stou64("64"), 64);
        EXPECT_EQ(co::stou64("8t"), 8ULL << 40);

        EXPECT_EQ(co::stod("3.14159"), 3.14159);

        int e;
        EXPECT_EQ(co::stob("xxx", &e), false);
        EXPECT_EQ(e, EINVAL);

        EXPECT_EQ(co::stoi32("12345678900", &e), 0);
        EXPECT_EQ(e, ERANGE);
        EXPECT_EQ(co::stoi32("-32g, &e"), 0);
        EXPECT_EQ(e, ERANGE);
        EXPECT_EQ(co::stoi32("-3a2", &e), 0);
        EXPECT_EQ(e, EINVAL);

        EXPECT_EQ(co::stoi64("1234567890123456789000", &e), 0);
        EXPECT_EQ(e, ERANGE);
        EXPECT_EQ(co::stoi64("100000P", &e), 0);
        EXPECT_EQ(e, ERANGE);
        EXPECT_EQ(co::stoi64("1di8", &e), 0);
        EXPECT_EQ(e, EINVAL);

        EXPECT_EQ(co::stou32("123456789000", &e), 0);
        EXPECT_EQ(e, ERANGE);
        EXPECT_EQ(co::stou32("32g", &e), 0);
        EXPECT_EQ(e, ERANGE);
        EXPECT_EQ(co::stou32("3g3", &e), 0);
        EXPECT_EQ(e, EINVAL);

        EXPECT_EQ(co::stou64("1234567890123456789000", &e), 0);
        EXPECT_EQ(e, ERANGE);
        EXPECT_EQ(co::stou64("100000P", &e), 0);
        EXPECT_EQ(e, ERANGE);
        EXPECT_EQ(co::stou64("12d8", &e), 0);
        EXPECT_EQ(e, EINVAL);

        EXPECT_EQ(co::stod("3.141d59", &e), 0);
        EXPECT_EQ(e, EINVAL);

        EXPECT_EQ(co::stou32("32", &e), 32);
        EXPECT_EQ(e, 0);
    }

    DEF_case(to_string) {
        EXPECT_EQ(co::to_string(3.14), "3.14");
        EXPECT_EQ(co::to_string(false), "false");
        EXPECT_EQ(co::to_string(true), "true");
        EXPECT_EQ(co::to_string(1024), "1024");
        EXPECT_EQ(co::to_string(-1024), "-1024");
    }
}

DEF_test(strmod) {
    DEF_case(split) {
        auto v = co::split("x||y", '|');
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "x");
        EXPECT_EQ(v[1], "");
        EXPECT_EQ(v[2], "y");

        v = co::split(co::string("x||y"), '|');
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "x");
        EXPECT_EQ(v[1], "");
        EXPECT_EQ(v[2], "y");

        v = co::split("x||y", '|', 1);
        EXPECT_EQ(v[0], "x");
        EXPECT_EQ(v[1], "|y");

        v = co::split("x y", ' ');
        EXPECT_EQ(v.size(), 2);
        EXPECT_EQ(v[0], "x");
        EXPECT_EQ(v[1], "y");

        v = co::split("\nx\ny\n", '\n');
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "");
        EXPECT_EQ(v[1], "x");
        EXPECT_EQ(v[2], "y");

        v = co::split(co::string("\nx\ny\n"), '\n');
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "");
        EXPECT_EQ(v[1], "x");
        EXPECT_EQ(v[2], "y");

        v = co::split("||x||y||", "||");
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "");
        EXPECT_EQ(v[1], "x");
        EXPECT_EQ(v[2], "y");

        v = co::split("||x||y||", "||", 2);
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "");
        EXPECT_EQ(v[1], "x");
        EXPECT_EQ(v[2], "y||");

        co::string s("||x||y||");
        v = co::split(s.data(), s.size(), "||", 2, 2);
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], "");
        EXPECT_EQ(v[2], "y||");
    }

    DEF_case(replace) {
        EXPECT_EQ(co::replace("$@xx$@", "$@", "#"), "#xx#");
        EXPECT_EQ(co::replace("$@xx$@", "$@", "#", 1), "#xx$@");
    }

    DEF_case(remove_outer|remove_prefix|remove_suffix) {
        const char* x = "aabbcc";
        co::string s(x);

        EXPECT_EQ(co::remove_outer(x, 2), "bb")
        EXPECT_EQ(co::remove_outer(s, 2), "bb")
        EXPECT_EQ(co::remove_prefix(x, 2), "bbcc")
        EXPECT_EQ(co::remove_prefix(s, 2), "bbcc")
        EXPECT_EQ(co::remove_suffix(x, 2), "aabb")
        EXPECT_EQ(co::remove_suffix(s, 2), "aabb")

        EXPECT_EQ(co::remove_outer(x, 3), "")
        EXPECT_EQ(co::remove_outer(x, 4), "")
        EXPECT_EQ(co::remove_outer(s, 3), "")
        EXPECT_EQ(co::remove_outer(s, 4), "")

        EXPECT_EQ(co::remove_prefix(x, 6), "")
        EXPECT_EQ(co::remove_prefix(x, 7), "")
        EXPECT_EQ(co::remove_prefix(s, 6), "")
        EXPECT_EQ(co::remove_prefix(s, 7), "")

        EXPECT_EQ(co::remove_suffix(x, 6), "")
        EXPECT_EQ(co::remove_suffix(x, 7), "")
        EXPECT_EQ(co::remove_suffix(s, 6), "")
        EXPECT_EQ(co::remove_suffix(s, 7), "")

        EXPECT_EQ(co::remove_prefix("abc.xx", "abc"), ".xx")
        EXPECT_EQ(co::remove_prefix(co::string("abc.xx"), "abc"), ".xx")
        EXPECT_EQ(co::remove_suffix("abc.xx", ".xx"), "abc")
        EXPECT_EQ(co::remove_suffix(co::string("abc.xx"), ".xx"), "abc")
        EXPECT_EQ(co::remove_prefix("abc.xx", "abx"), "abc.xx")
        EXPECT_EQ(co::remove_prefix(co::string("abc.xx"), "abx"), "abc.xx")
        EXPECT_EQ(co::remove_suffix("abc.xx", "abx"), "abc.xx")
        EXPECT_EQ(co::remove_suffix(co::string("abc.xx"), "abx"), "abc.xx")
    }

    DEF_case(trim|trim_left|trim_right) {
        EXPECT_EQ(co::trim(" \txx\t  \n"), "xx");
        EXPECT_EQ(co::trim("$@xx@", "$@"), "xx");
        EXPECT_EQ(co::trim_left("$@xx@", "$@"), "xx@");
        EXPECT_EQ(co::trim_right("$@xx@", "$@"), "$@xx");
        EXPECT_EQ(co::trim("xx", ""), "xx");
        EXPECT_EQ(co::trim("", "xx"), "");
        EXPECT_EQ(co::trim("$@xx@", '$'), "@xx@");
        EXPECT_EQ(co::trim_left("$@xx@", '$'), "@xx@");
        EXPECT_EQ(co::trim_right("$@xx@", '$'), "$@xx@");
        EXPECT_EQ(co::trim("$@xx@", '@'), "$@xx");
        EXPECT_EQ(co::trim("", '\0'), "");

        EXPECT_EQ(co::trim(co::string("$@xx@"), "$@"), "xx");
        EXPECT_EQ(co::trim_left(co::string("$@xx@"), "$@"), "xx@");
        EXPECT_EQ(co::trim_right(co::string("$@xx@"), "$@"), "$@xx");
        EXPECT_EQ(co::trim(co::string("$@xx@"), '$'), "@xx@");
        EXPECT_EQ(co::trim_right(co::string("$@xx@"), '$'), "$@xx@");
        EXPECT_EQ(co::trim_left(co::string("$@xx@"), '$'), "@xx@");
        EXPECT_EQ(co::trim(co::string("$@xx@"), '@'), "$@xx");
        EXPECT_EQ(co::trim(co::string("\0xx\0", 4), '\0'), "xx");
    }
}

} // namespace test
