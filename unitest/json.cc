#include "co/unitest.h"
#include "co/json.h"
#include "co/str.h"

namespace test {

DEF_test(json) {
    DEF_case(null) {
        Json n;
        EXPECT(n.is_null());
        EXPECT_EQ(n.str(), "null");
        EXPECT_EQ(n.pretty(), "null");
    }

    DEF_case(bool) {
        Json b = true;
        EXPECT(b.is_bool());
        EXPECT_EQ(b.get_bool(), true);
        EXPECT_EQ(b.str(), "true");
        EXPECT_EQ(b.pretty(), "true");

        b = false;
        EXPECT_EQ(b.get_bool(), false);
        EXPECT_EQ(b.str(), "false");
        EXPECT_EQ(b.pretty(), "false");
    }

    DEF_case(int) {
        Json i = 0;
        EXPECT(i.is_int());
        EXPECT_EQ(i.get_int(), 0);
        EXPECT_EQ(i.str(), "0");
        EXPECT_EQ(i.pretty(), "0");

        i = 123;
        EXPECT_EQ(i.get_int(), 123);
        EXPECT_EQ(i.str(), "123");
        EXPECT_EQ(i.pretty(), "123");

        Json i64 = (int64) 12345;
        EXPECT(i64.is_int());
        EXPECT_EQ(i64.get_int64(), 12345);
        EXPECT_EQ(i64.str(), "12345");
        EXPECT_EQ(i64.pretty(), "12345");
    }

    DEF_case(double) {
        Json d = 3.14;
        EXPECT(d.is_double());
        EXPECT_EQ(d.get_double(), 3.14);
        EXPECT_EQ(d.str(), "3.14");
        EXPECT_EQ(d.pretty(), "3.14");

        d = 7e-5;
        EXPECT(d.is_double());
        EXPECT_EQ(d.get_double(), 7e-5);

        d = 1.2e5;
        EXPECT(d.is_double());
        EXPECT_EQ(d.get_double(), 1.2e5);
    }

    DEF_case(string) {
        Json s = "hello world";
        EXPECT(s.is_string());
        EXPECT_EQ(s.size(), 11);
        EXPECT_EQ(s.str(), "\"hello world\"");
        EXPECT_EQ(s.pretty(), "\"hello world\"");

        s = fastring(200, 'x').append('\n').append(99, 'x');
        EXPECT(s.is_string());
        EXPECT_EQ(s.size(), 300);
        EXPECT_EQ(s.str(), fastring("\"").append(200, 'x').append("\\n").append(99, 'x').append('"'));
        EXPECT_EQ(s.dbg(), fastring("\"").append(200, 'x').append("\\n").append(55, 'x').append(3, '.').append('"'));

        s = fastring(300, 'x');
        EXPECT_EQ(s.str(), fastring("\"").append(300, 'x').append('"'));
        EXPECT_EQ(s.dbg(), fastring("\"").append(256, 'x').append(3, '.').append('"'));

        s = fastring("hello world");
        EXPECT(s.is_string());
        EXPECT_EQ(s.size(), 11);
        EXPECT_EQ(s.str(), "\"hello world\"");
        EXPECT_EQ(s.pretty(), "\"hello world\"");

        Json t = s;
        t = "xxx";
        EXPECT_EQ(t.str(), "\"xxx\"");
        EXPECT_EQ(s.str(), "\"hello world\"");
    }

    DEF_case(array) {
        Json a = json::array();
        EXPECT(a.is_array());
        EXPECT(a.empty());
        EXPECT_EQ(a.str(), "[]");
        EXPECT_EQ(a.pretty(), "[]");

        Json x = a;
        for (int i = 0; i < 10; ++i) x.push_back(i);
        EXPECT_EQ(x.str(), "[0,1,2,3,4,5,6,7,8,9]");
        EXPECT_EQ(a.str(), "[0,1,2,3,4,5,6,7,8,9]");

        Json v;
        v.push_back(1);
        v.push_back("hello");
        v.push_back(1.23);
        EXPECT(v.is_array());
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v.str(), "[1,\"hello\",1.23]");
    }

    DEF_case(object) {
        Json o = json::object();
        EXPECT(o.is_object());
        EXPECT(o.empty());
        EXPECT_EQ(o.str(), "{}");
        EXPECT_EQ(o.pretty(), "{}");

        Json v;
        v["name"] = "vin";
        v["age"] = 29;
        v.add_member("phone", "1234567");
        EXPECT(v.is_object());
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v.str(), "{\"name\":\"vin\",\"age\":29,\"phone\":\"1234567\"}");

        Json u = json::parse(v.str());
        EXPECT(u.is_object());
        EXPECT_EQ(u.size(), 3);

        u = json::parse(v.pretty());
        EXPECT(u.is_object());
        EXPECT_EQ(u.size(), 3);

        Json p = o;
        p.add_member("0", 0);
        p.add_member("1", 1);
        p.add_member("2", 2);
        p.add_member("3", 3);
        p.add_member("4", 4);
        p.add_member("5", 5);
        p.add_member("6", 6);
        p.add_member("7", 7);
        p.add_member("8", 8);
        EXPECT_EQ(p.str(), o.str());
        EXPECT_EQ(*(void**)&o, *(void**)&p);
    }

    DEF_case(parse_null) {
        Json v;
        EXPECT(v.parse_from("null"));
        EXPECT(v.is_null());
    }

    DEF_case(parse_bool) {
        Json v = json::parse("false");
        EXPECT(v.is_bool());
        EXPECT_EQ(v.get_bool(), false);

        v = json::parse("true");
        EXPECT(v.is_bool());
        EXPECT_EQ(v.get_bool(), true);
    }

    DEF_case(parse_int) {
        Json v = json::parse("32");
        EXPECT(v.is_int());
        EXPECT_EQ(v.get_int(), 32);

        v = json::parse("-32");
        EXPECT_EQ(v.get_int(), -32);

        v = json::parse(str::from(MAX_UINT64));
        EXPECT(v.is_int());
        EXPECT_EQ(v.get_uint64(), MAX_UINT64);

        v = json::parse(str::from(MIN_INT64));
        EXPECT(v.is_int());
        EXPECT_EQ(v.get_int64(), MIN_INT64);

        EXPECT_EQ(json::parse("18446744073709551614").get_uint64(), MAX_UINT64 - 1);
        EXPECT_EQ(json::parse("1844674407370955161").get_uint64(), 1844674407370955161ULL);
        EXPECT_EQ(json::parse("-9223372036854775807").get_uint64(), MIN_INT64 + 1);

        fastring s("1234567");
        s.resize(3);
        EXPECT_EQ(json::parse(s).get_int32(), 123);

        EXPECT(json::parse("--3").is_null());
        EXPECT(json::parse("+3").is_null());
        EXPECT(json::parse("03").is_null());
        EXPECT(json::parse("2a").is_null());
        EXPECT(json::parse("2 3").is_null());
        EXPECT(json::parse("1234567890 123456789").is_null());
    }

    DEF_case(parse_double) {
        Json v = json::parse("0.3");
        EXPECT(v.is_double());
        EXPECT_EQ(v.get_double(), 0.3);

        v = json::parse("7e-5");
        EXPECT(v.is_double());
        EXPECT_EQ(v.get_double(), 7e-5);

        v = json::parse("1.2e5");
        EXPECT(v.is_double());
        EXPECT_EQ(v.get_double(), 1.2e5);

        EXPECT_EQ(json::parse("1.0000000000000002").get_double(), 1.0000000000000002);
        EXPECT_EQ(json::parse("4.9406564584124654e-324").get_double(), 4.9406564584124654e-324);
        EXPECT_EQ(json::parse("1.7976931348623157e+308").get_double(), 1.7976931348623157e+308);
        EXPECT(json::parse("18446744073709551616").is_double()); // MAX_UINT64 + 1
        EXPECT(json::parse("-9223372036854775809").is_double()); // MIN_INT64 - 1

        fastring s("1234.5678");
        s.resize(6);
        EXPECT_EQ(json::parse(s).get_double(), 1234.5);

        EXPECT(json::parse(".123").is_null());
        EXPECT(json::parse("123.").is_null());
        EXPECT(json::parse("inf").is_null());
        EXPECT(json::parse("nan").is_null());
        EXPECT(json::parse("0.2.2").is_null());
        EXPECT(json::parse("0.2a").is_null());
        EXPECT(json::parse("0.2e").is_null());
        EXPECT(json::parse("0.2e+").is_null());
    }

    DEF_case(parse_array) {
        Json v = json::parse("[]");
        EXPECT(v.is_array());
        EXPECT_EQ(v.str(), "[]");

        v = json::parse("[ 1, 2 , \"hello\", [3,4,5] ]");
        EXPECT(v.is_array());
        EXPECT_EQ(v.size(), 4);
        EXPECT_EQ(v[0].get_int(), 1);
        EXPECT_EQ(v[1].get_int(), 2);
        EXPECT_EQ(v[2].get_string(), fastring("hello"));
        EXPECT(v[3].is_array());
        EXPECT_EQ(v[3][0].get_int(), 3);
    }

    DEF_case(parse_object) {
        Json v = json::parse("{}");
        EXPECT(v.is_object());
        EXPECT_EQ(v.str(), "{}");

        v = json::parse("{\"key\": []}");
        EXPECT(v.is_object());
        EXPECT(v["key"].is_array());
        EXPECT_EQ(v["key"].str(), "[]");

        v = json::parse("{\"key\": {}}");
        EXPECT(v.is_object());
        EXPECT(v["key"].is_object());
        EXPECT_EQ(v["key"].str(), "{}");

        v = json::parse("{ \"key\" : null }");
        EXPECT(v.is_object());
        EXPECT(v["key"].is_null());

        fastring ss = "{ \"hello\":23, \"world\": { \"xxx\": 99 } }";
        v = json::parse(ss.data(), ss.size());
        EXPECT(v.is_object());
        EXPECT_EQ(v["hello"].get_int(), 23);

        Json u = v["world"];
        v = Json();
        EXPECT_EQ(u["xxx"].str(), "99");
    }

    DEF_case(parse_escape) {
        Json v;
        v.parse_from("{ \"a\":23, \n \r \t  \"b\":\"str\", \r\n }");
        EXPECT_EQ(v.str(), "{\"a\":23,\"b\":\"str\"}");

        v = json::parse("{ \"s\":\"\\\\s\" }");
        EXPECT_EQ(v.str(), "{\"s\":\"\\\\s\"}")

        v = json::parse("{ \"s\":\"s\\\"\" }");
        EXPECT_EQ(v.str(), "{\"s\":\"s\\\"\"}");

        v = json::parse("{ \"key\": \"\\/\\r\\n\\t\\b\\f\" }");
        EXPECT_EQ(fastring(v["key"].get_string()), "/\r\n\t\b\f");

        v = json::parse("{ \"key\": \"\u4e2d\u56fd\u4eba\" }");
        EXPECT_EQ(fastring(v["key"].get_string()), "中国人");
    }

    DEF_case(parse_error) {
        Json v;
        v.parse_from("");
        EXPECT(v.is_null());

        EXPECT(json::parse("").is_null());
        EXPECT(json::parse("{").is_null());
        EXPECT(json::parse("[").is_null());
        EXPECT(json::parse("{\"key\":23 45}").is_null());
        EXPECT(json::parse("{\"key\": 23,").is_null());
        EXPECT(json::parse("{\"key\": ,}").is_null());
        EXPECT(json::parse("{ \"key\" : 23 } xx").is_null());
        EXPECT(json::parse("{ \"key\" : 23 }  { \"key\" : 23 }").is_null());
        EXPECT(json::parse("{ \"key\" : false88 }").is_null());
        EXPECT(json::parse("{ \"key\" : true88 }").is_null());
        EXPECT(json::parse("{ \"key\" : null88 }").is_null());
        EXPECT(json::parse("{ \"key\" : abcc }").is_null());
    }
}

} // namespace test
