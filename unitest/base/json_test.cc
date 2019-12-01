#include "base/unitest.h"
#include "base/json.h"

namespace test {

DEF_test(json) {
    DEF_case(base) {
        Json n;
        EXPECT(n.is_null());
        EXPECT_EQ(n.str(), "null");
        EXPECT_EQ(n.pretty(), "null");

        Json z = 0;
        EXPECT(z.is_int());
        EXPECT_EQ(z.get_int(), 0);
        EXPECT_EQ(z.str(), "0");
        EXPECT_EQ(z.pretty(), "0");

        Json i = 123;
        EXPECT(i.is_int());
        EXPECT_EQ(i.get_int(), 123);
        EXPECT_EQ(i.str(), "123");
        EXPECT_EQ(i.pretty(), "123");

        Json i64 = (int64) 12345;
        EXPECT(i64.is_int());
        EXPECT_EQ(i64.get_int64(), 12345);
        EXPECT_EQ(i64.str(), "12345");
        EXPECT_EQ(i64.pretty(), "12345");

        Json b = true;
        EXPECT(b.is_bool());
        EXPECT_EQ(b.get_bool(), true);
        EXPECT_EQ(b.str(), "true");
        EXPECT_EQ(b.pretty(), "true");

        Json d = 3.14;
        EXPECT(d.is_double());
        EXPECT_EQ(d.get_double(), 3.14);
        EXPECT_EQ(d.str(), "3.14");
        EXPECT_EQ(d.pretty(), "3.14");

        Json cs = "hello world";
        EXPECT(cs.is_string());
        EXPECT_EQ(cs.str(), "\"hello world\"");
        EXPECT_EQ(cs.pretty(), "\"hello world\"");

        Json s = fastring("hello world");
        EXPECT(s.is_string());
        EXPECT_EQ(s.str(), "\"hello world\"");
        EXPECT_EQ(s.pretty(), "\"hello world\"");

        Json a = json::array();
        EXPECT(a.is_array());
        EXPECT(a.empty());
        EXPECT_EQ(a.str(), "[]");
        EXPECT_EQ(a.pretty(), "[]");

        Json o = json::object();
        EXPECT(o.is_object());
        EXPECT(o.empty());
        EXPECT_EQ(o.str(), "{}");
        EXPECT_EQ(o.pretty(), "{}");
    }

    DEF_case(array) {
        Json v;
        v.push_back(1);
        v.push_back("hello");
        v.push_back(1.23);

        EXPECT(v.is_array());
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v.str(), "[1,\"hello\",1.23]");
    }

    DEF_case(object) {
        Json v;
        v["name"] = "vin";
        v["age"] = 29;
        v["phone"] = "1234567";

        EXPECT(v.is_object());
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v.str(), "{\"name\":\"vin\",\"age\":29,\"phone\":\"1234567\"}");

        Json u = json::parse(v.str());
        EXPECT(u.is_object());
        EXPECT_EQ(u.size(), 3);

        u = json::parse(v.pretty());
        EXPECT(u.is_object());
        EXPECT_EQ(u.size(), 3);
    }

    DEF_case(parse) {
        EXPECT(json::parse("{").is_null());
        Json v;

        v.parse_from("");
        EXPECT(v.is_null());

        v = json::parse("{}");
        EXPECT_EQ(v.str(), "{}");

        v.parse_from("{ \"a\":23, \n \r \t  \"b\":\"str\", \r\n }");
        EXPECT_EQ(v.str(), "{\"a\":23,\"b\":\"str\"}");

        v = json::parse("{ \"s\":\"\\\\s\" }");
        EXPECT_EQ(v.str(), "{\"s\":\"\\\\s\"}")

        v = json::parse("{ \"s\":\"s\\\"\" }");
        EXPECT_EQ(v.str(), "{\"s\":\"s\\\"\"}");

        v = json::parse("{\"key\":23 45}");
        EXPECT(v.is_null());

        v = json::parse("{\"key\": 23 }");
        EXPECT(v.is_object());
        EXPECT_EQ(v["key"].get_int(), 23);

        v = json::parse("{ \"key\" : null }");
        EXPECT(v.is_object());
        EXPECT(v["key"].is_null());

        v = json::parse("{ \"key\" : 23 } xx");
        EXPECT(v.is_null());

        v = json::parse("{ \"key\" : 23 }  { \"key\" : 23 }");
        EXPECT(v.is_null());

        v = json::parse("{ \"key\" : false88 }");
        EXPECT(v.is_null());

        v = json::parse("{ \"key\" : true88 }");
        EXPECT(v.is_null());

        v = json::parse("{ \"key\" : null88 }");
        EXPECT(v.is_null());

        v = json::parse("{ \"key\" : abcc }");
        EXPECT(v.is_null());

        v = json::parse("{ \"key\" : false }");
        EXPECT_EQ(v["key"].get_bool(), false);

        v = json::parse("{ \"key\" : true }");
        EXPECT_EQ(v["key"].get_bool(), true);

        v = json::parse("{ \"key\": \"\\/\\r\\n\\t\\b\\f\" }");
        EXPECT_EQ(fastring(v["key"].get_string()), "/\r\n\t\b\f");

        v = json::parse("{ \"key\": \"\u4e2d\u56fd\u4eba\" }");
        EXPECT_EQ(fastring(v["key"].get_string()), "中国人");
    }
}

} // namespace test
