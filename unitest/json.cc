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
        EXPECT_EQ(n.as_bool(), false);
        EXPECT_EQ(n.as_int(), 0);
        EXPECT_EQ(n.as_double(), 0);
        EXPECT_EQ(n.as_string(), "null");
        EXPECT_EQ(n.string_size(), 0);
        EXPECT_EQ(n.array_size(), 0);
        EXPECT_EQ(n.object_size(), 0);
    }

    DEF_case(bool) {
        Json b = true;
        EXPECT(b.is_bool());
        EXPECT(b == true);
        EXPECT(b != false);
        EXPECT_EQ(b.as_bool(), true);
        EXPECT_EQ(b.as_int(), 1);
        EXPECT_EQ(b.as_double(), 1.0);
        EXPECT_EQ(b.as_string(), "true");
        EXPECT_EQ(b.str(), "true");
        EXPECT_EQ(b.pretty(), "true");

        b = false;
        EXPECT_EQ(b.as_bool(), false);
        EXPECT_EQ(b.as_int(), 0);
        EXPECT_EQ(b.as_double(), 0);
        EXPECT_EQ(b.as_string(), "false");
        EXPECT_EQ(b.str(), "false");
        EXPECT_EQ(b.pretty(), "false");

        b.reset();
        EXPECT(b.is_null());
    }

    DEF_case(int) {
        Json i = 0;
        EXPECT(i.is_int());
        EXPECT(i == 0);
        EXPECT(i != 1);
        EXPECT_EQ(i.as_int(), 0);
        EXPECT_EQ(i.as_bool(), false);
        EXPECT_EQ(i.as_double(), 0);
        EXPECT_EQ(i.as_string(), "0");
        EXPECT_EQ(i.str(), "0");
        EXPECT_EQ(i.pretty(), "0");

        i = 123;
        EXPECT_EQ(i.as_int(), 123);
        EXPECT_EQ(i.as_bool(), true);
        EXPECT_EQ(i.as_double(), 123.0);
        EXPECT_EQ(i.as_string(), "123");
        EXPECT_EQ(i.str(), "123");
        EXPECT_EQ(i.pretty(), "123");

        Json x = (int64)12345;
        EXPECT(x.is_int());
        EXPECT(x == (int64)12345);
        EXPECT_EQ(x.as_int64(), 12345);
        EXPECT_EQ(x.as_int64(), 12345);
        EXPECT_EQ(x.str(), "12345");
        EXPECT_EQ(x.pretty(), "12345");

        x.reset();
        EXPECT(x.is_null());
    }

    DEF_case(double) {
        Json d = 3.14;
        EXPECT(d.is_double());
        EXPECT(d == 3.14);
        EXPECT_EQ(d.as_double(), 3.14);
        EXPECT_EQ(d.as_bool(), true);
        EXPECT_EQ(d.as_int(), 3);
        EXPECT_EQ(d.as_string(), "3.14");
        EXPECT_EQ(d.str(), "3.14");
        EXPECT_EQ(d.pretty(), "3.14");

        d = 7e-5;
        EXPECT(d == 7e-5);
        EXPECT(d.is_double());
        EXPECT_EQ(d.as_double(), 7e-5);
        EXPECT_EQ(d.as_int(), 0);

        d = 1.2e5;
        EXPECT(d.is_double());
        EXPECT_EQ(d.as_double(), 1.2e5);
    }

    DEF_case(string) {
        Json s = "hello world";
        EXPECT(s.is_string());
        EXPECT(s == "hello world");
        EXPECT(s == fastring("hello world"));
        EXPECT_EQ(s.size(), 11);
        EXPECT_EQ(s.string_size(), 11);
        EXPECT_EQ(s.as_string(), "hello world");
        EXPECT_EQ(s.str(), "\"hello world\"");
        EXPECT_EQ(s.pretty(), "\"hello world\"");

        s = fastring(30, 'x').append('\n').append(500, 'x');
        EXPECT(s.is_string());
        EXPECT_EQ(s.size(), 531);
        EXPECT_EQ(s.str(), fastring("\"").append(30, 'x').append("\\n").append(500, 'x').append('"'));
        EXPECT_EQ(s.dbg(), fastring("\"").append(30, 'x').append("\\n").append(1, 'x').append(3, '.').append('"'));

        s = fastring(600, 'x');
        EXPECT_EQ(s.str(), fastring("\"").append(600, 'x').append('"'));
        EXPECT_EQ(s.dbg(), fastring("\"").append(32, 'x').append(3, '.').append('"'));

        s = fastring("hello world");
        EXPECT(s.is_string());
        EXPECT_EQ(s.size(), 11);
        EXPECT_EQ(s.str(), "\"hello world\"");
        EXPECT_EQ(s.pretty(), "\"hello world\"");

        s = "123";
        EXPECT_EQ(s.as_int(), 123);
        EXPECT_EQ(s.as_double(), 123.0);

        s = "false";
        EXPECT_EQ(s.as_bool(), false);
        s = "0";
        EXPECT_EQ(s.as_bool(), false);

        s = "true";
        EXPECT_EQ(s.as_bool(), true);
        s = "1";
        EXPECT_EQ(s.as_bool(), true);
    }

    DEF_case(operator=) {
        Json s = "hello world";
        EXPECT(s.is_string());
        
        s = 1;
        EXPECT(s.is_int());
        EXPECT_EQ(s.as_int(), 1);

        s = false;
        EXPECT(s.is_bool());
        EXPECT_EQ(s.as_bool(), false);

        s = 3.14;
        EXPECT(s.is_double());
        EXPECT_EQ(s.as_double(), 3.14);

        s = "xxx";
        EXPECT(s.is_string());
        EXPECT_EQ(s.as_string(), "xxx");
    }

    DEF_case(copy) {
        Json x = 1;
        Json y = x;
        EXPECT(x.is_null());
        EXPECT_EQ(y.as_int(), 1);

        Json o;
        o.add_member("y", y);
        EXPECT(y.is_null());
        EXPECT_EQ(o["y"].as_int(), 1);

        Json a;
        a.push_back(o);
        EXPECT(o.is_null());
        EXPECT_EQ(a[0]["y"].as_int(), 1);

        o.add_member("a", a);
        EXPECT(a.is_null());
        EXPECT_EQ(o["a"][0]["y"].as_int(), 1);
    }

    DEF_case(initializer_list) {
        Json a = { 1, 2, 3 };
        EXPECT(a.is_array());
        EXPECT_EQ(a.size(), 3);

        Json o = {
            { "x", 3 },
            { "y", 7 },
            { "z", { 1, 2, 3 } },
        };
        EXPECT(o.is_object());
        EXPECT_EQ(o["x"].as_int(), 3);
        EXPECT_EQ(o["z"][0].as_int(), 1);
        EXPECT_EQ(o["z"][1].as_int(), 2);
        EXPECT_EQ(o["z"][2].as_int(), 3);

        a = json::array({
            { "x", 3 },
            { "y", 7 },
        });
        EXPECT(a.is_array());
        EXPECT_EQ(a.size(), 2);
        EXPECT_EQ(a[0][1].as_int(), 3);
        EXPECT_EQ(a[1][1].as_int(), 7);
        EXPECT_EQ(a[0][0].as_string(), "x");
        EXPECT_EQ(a[1][0].as_string(), "y");
    }
 
    DEF_case(dup) {
        Json x = {
            1, "xxx", 3.14
        };
        Json y = x.dup();

        EXPECT(!x.is_null());
        EXPECT(!y.is_null());
        EXPECT_EQ(x.str(), y.str());

        Json o = {
            { "x", 3 },
            { "y", "888" },
            { "z", { 1, 2, 3 } },
        };

        Json v = o.dup();
        EXPECT(!o.is_null());
        EXPECT_EQ(o.str(), v.str());
    }
   
    DEF_case(array) {
        Json a = json::array();
        EXPECT(a.is_array());
        EXPECT_EQ(a.str(), "[]");
        EXPECT_EQ(a.pretty(), "[]");

        for (int i = 0; i < 10; ++i) a.push_back(i);
        EXPECT_EQ(a.str(), "[0,1,2,3,4,5,6,7,8,9]");
        EXPECT_EQ(a[0].as_int(), 0);
        EXPECT_EQ(a[9].as_int(), 9);

        Json v;
        v.push_back(1);
        v.push_back("hello");
        v.push_back(1.23);
        EXPECT(v.is_array());
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v.array_size(), 3);
        EXPECT_EQ(v.str(), "[1,\"hello\",1.23]");

        v.add_member("x", 23);
        EXPECT(v.is_object());
        EXPECT_EQ(v["x"].as_int(), 23);
    }

    DEF_case(object) {
        Json o = json::object();
        EXPECT(o.is_object());
        EXPECT_EQ(o.str(), "{}");
        EXPECT_EQ(o.pretty(), "{}");

        Json v;
        v.add_member("name", "vin");
        v.add_member("age", 29);
        v.add_member("phone", "1234567");
        EXPECT(v.is_object());
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v.str(), "{\"name\":\"vin\",\"age\":29,\"phone\":\"1234567\"}");
        EXPECT_EQ(v["name"].as_string(), "vin");
        EXPECT_EQ(v["age"].as_int(), 29);

        Json u = json::parse(v.str());
        EXPECT(u.is_object());
        EXPECT_EQ(u.size(), 3);
        EXPECT_EQ(u["name"].as_string(), "vin");
        EXPECT_EQ(u["age"].as_int(), 29);

        Json x = json::parse(v.pretty());
        EXPECT(x.is_object());
        EXPECT_EQ(x.size(), 3);
        EXPECT_EQ(x["name"].as_string(), "vin");
        EXPECT_EQ(x["age"].as_int(), 29);

        o.add_member("0", 0);
        o.add_member("1", 1);
        o.add_member("2", 2);
        o.add_member("3", 3);
        o.add_member("4", 4);
        o.add_member("5", 5);
        o.add_member("6", 6);
        o.add_member("7", 7);
        o.add_member("8", 8);
        o.add_member("9", 9);
        EXPECT_EQ(o.size(), 10);
        EXPECT_EQ(o.object_size(), 10);
        EXPECT_EQ(o["9"].as_int(), 9);

        Json a = { 1, 2, 3 };
        o.add_member("a", a);
        EXPECT(o["a"].is_array());
        EXPECT_EQ(o["a"][0].as_int(), 1);

        o.push_back(1).push_back(2).push_back(3);
        EXPECT(o.is_array());
        EXPECT_EQ(o.array_size(), 3);
        EXPECT_EQ(o[1].as_int(), 2);
    }

    DEF_case(has_member) {
        Json v;
        v.add_member("apple", "666");
        EXPECT(v.has_member("apple"));
        EXPECT(!v.has_member("666"));
        EXPECT_EQ(v["apple"].as_string(), "666");
    }

    DEF_case(get) {
        Json o = {
            { "x", 3 },
            { "y", 7 },
            { "z", { 1, 2, 3 } },
        };
        EXPECT(o.get("xxx").is_null());
        EXPECT(o.get("xxx", 1, "xx").is_null());
        EXPECT_EQ(o.get("x").as_int(), 3);
        EXPECT_EQ(o.get("y").as_int(), 7);
        EXPECT(o.get("z").is_array());
        EXPECT_EQ(o.get("z", 0).as_int(), 1);
        EXPECT_EQ(o.get("z", 1).as_int(), 2);
        EXPECT_EQ(o.get("z", 2).as_int(), 3);
        EXPECT(o.get("z", 3).is_null());
    }

    DEF_case(set) {
        // {"a":1,"b":[0,1,2],"c":{"d":["oo"]}}
        Json x;
        x.set("a", 1);
        x.set("b", Json({ 0,1,2 }));
        x.set("c", "d", 0, "oo");
        EXPECT_EQ(x.get("a").as_int(), 1);
        EXPECT_EQ(x.get("b", 0).as_int(), 0);
        EXPECT_EQ(x.get("b", 2).as_int(), 2);
        EXPECT(x.get("c", "d", 0) == "oo");

        x.set("a", false);
        EXPECT_EQ(x.get("a").as_bool(), false);

        x.set("b", 3);
        EXPECT_EQ(x.get("b").as_int(), 3);

        x.set("a", 1, 23);
        EXPECT(x.get("a", 0).is_null());
        EXPECT_EQ(x.get("a", 1).as_int(), 23);

        x.set("a", 0, 7);
        EXPECT_EQ(x.get("a", 0).as_int(), 7);
        x.set("a", 3, 88);
        EXPECT(x.get("a", 2).is_null());
        EXPECT_EQ(x.get("a", 3).as_int(), 88);
    }

    DEF_case(remove) {
        Json x = {
            { "a", 1 },
            { "b", 2 },
            { "c", {1,2,3} },
        };

        x.remove("a");
        EXPECT_EQ(x.object_size(), 2);
        {
            auto it = x.begin();
            EXPECT(strcmp(it.key(), "c") == 0);
        }

        x.remove("b");
        EXPECT_EQ(x.object_size(), 1);

        auto& c = x.get("c");
        EXPECT(c.is_array() && c.array_size() == 3);
        c.remove(0);
        EXPECT_EQ(c.array_size(), 2);
        EXPECT_EQ(c[0].as_int(), 3);
        EXPECT_EQ(c[1].as_int(), 2);
        c.remove(1);
        EXPECT_EQ(c.array_size(), 1);
        EXPECT_EQ(c[0].as_int(), 3);
    }

    DEF_case(erase) {
        Json x = {
            { "a", 1 },
            { "b", 2 },
            { "c", {1,2,3} },
        };

        x.erase("a");
        EXPECT_EQ(x.object_size(), 2);
        {
            auto it = x.begin();
            EXPECT(strcmp(it.key(), "b") == 0);
        }

        x.erase("b");
        EXPECT_EQ(x.object_size(), 1);

        auto& c = x.get("c");
        EXPECT(c.is_array() && c.array_size() == 3);
        c.erase(0);
        EXPECT_EQ(c.array_size(), 2);
        EXPECT_EQ(c[0].as_int(), 2);
        EXPECT_EQ(c[1].as_int(), 3);
        c.erase(1);
        EXPECT_EQ(c.array_size(), 1);
        EXPECT_EQ(c[0].as_int(), 2);
    }

    DEF_case(iterator) {
        Json v;
        EXPECT(v.begin() == v.end());
        v = 3;
        EXPECT(v.begin() == v.end());
        v = 3.2;
        EXPECT(v.begin() == v.end());
        v = false;
        EXPECT(v.begin() == v.end());
        v = "hello";
        EXPECT(v.begin() == v.end());

        Json a;
        a.push_back(1);
        a.push_back(2);
        a.push_back(3);
        a.push_back(4);
        EXPECT_EQ(a.size(), 4);

        auto it = a.begin();
        EXPECT_EQ((*it).as_int(), 1);
        ++it; ++it; ++it;
        EXPECT_EQ((*it).as_int(), 4);
        ++it;
        EXPECT(it == a.end());
    }

    DEF_case(parse_null) {
        Json v;
        EXPECT(v.parse_from("null"));
        EXPECT(v.is_null());
    }

    DEF_case(parse_bool) {
        Json v = json::parse("false");
        EXPECT(v.is_bool());
        EXPECT_EQ(v.as_bool(), false);

        v = json::parse("true");
        EXPECT(v.is_bool());
        EXPECT_EQ(v.as_bool(), true);
    }

    DEF_case(parse_int) {
        Json v = json::parse("32");
        EXPECT(v.is_int());
        EXPECT_EQ(v.as_int(), 32);

        v = json::parse("-32");
        EXPECT_EQ(v.as_int(), -32);

        v = json::parse(str::from(MAX_UINT64));
        EXPECT(v.is_int());
        EXPECT_EQ(v.as_int64(), MAX_UINT64);

        v = json::parse(str::from(MIN_INT64));
        EXPECT(v.is_int());
        EXPECT_EQ(v.as_int64(), MIN_INT64);

        EXPECT_EQ(json::parse("18446744073709551614").as_int64(), MAX_UINT64 - 1);
        EXPECT_EQ(json::parse("1844674407370955161").as_int64(), 1844674407370955161ULL);
        EXPECT_EQ(json::parse("-9223372036854775807").as_int64(), MIN_INT64 + 1);

        fastring s("12345678901234567890888");
        s.resize(3);
        EXPECT_EQ(json::parse(s).as_int(), 123);

        EXPECT(json::parse("--3").is_null());
        EXPECT(json::parse("+3").is_null());
        EXPECT(json::parse("03").is_null());
        EXPECT(json::parse("2a").is_null());
    }

    DEF_case(parse_double) {
        Json v = json::parse("0.3");
        EXPECT(v.is_double());
        EXPECT_EQ(v.as_double(), 0.3);

        v = json::parse("7e-5");
        EXPECT(v.is_double());
        EXPECT_EQ(v.as_double(), 7e-5);

        v = json::parse("1.2e5");
        EXPECT(v.is_double());
        EXPECT_EQ(v.as_double(), 1.2e5);

        EXPECT_EQ(json::parse("1.0000000000000002").as_double(), 1.0000000000000002);
        EXPECT_EQ(json::parse("4.9406564584124654e-324").as_double(), 4.9406564584124654e-324);
        EXPECT_EQ(json::parse("1.7976931348623157e+308").as_double(), 1.7976931348623157e+308);
        EXPECT(json::parse("18446744073709551616").is_double()); // MAX_UINT64 + 1
        EXPECT(json::parse("-9223372036854775809").is_double()); // MIN_INT64 - 1

        fastring s("1234.567");
        s.resize(6);
        EXPECT_EQ(json::parse(s).as_double(), 1234.5);

        s = "1234.5 889";
        s.resize(6);
        EXPECT_EQ(json::parse(s).as_double(), 1234.5);

        EXPECT(json::parse(".123").is_null());
        EXPECT(json::parse("123.").is_null());
        EXPECT(json::parse("1.2e").is_null());
        EXPECT(json::parse("inf").is_null());
        EXPECT(json::parse("nan").is_null());
        EXPECT(json::parse("0.2.2").is_null());
        EXPECT(json::parse("0.2a").is_null());
        EXPECT(json::parse("0.2f").is_null());
        EXPECT(json::parse("123.4 56").is_null());
    }

    DEF_case(parse_string) {
        Json v = json::parse("\"\"");
        EXPECT(v.is_string());
        EXPECT_EQ(v.str(), "\"\"");

        v = json::parse("\"hello world\"");
        EXPECT(v.is_string());
        EXPECT_EQ(v.str(), "\"hello world\"");

        v = json::parse(fastring().append('"').append(300, 'x').append('"'));
        EXPECT(v.is_string());
        EXPECT_EQ(v.str(), fastring().append('"').append(300, 'x').append('"'));
    }

    DEF_case(parse_array) {
        Json v = json::parse("[]");
        EXPECT(v.is_array());
        EXPECT_EQ(v.str(), "[]");

        v = json::parse("[ 1, 2 , \"hello\", [3,4,5] ]");
        EXPECT(v.is_array());
        EXPECT_EQ(v.size(), 4);
        EXPECT_EQ(v[0].as_int(), 1);
        EXPECT_EQ(v[1].as_int(), 2);
        EXPECT_EQ(v[2].as_string(), "hello");
        EXPECT(v[3].is_array());
        EXPECT_EQ(v[3][0].as_int(), 3);

        fastring s("[]");
        s.resize(1);
        EXPECT(json::parse(s).is_null());
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
        EXPECT_EQ(v["hello"].as_int(), 23);

        Json& u = v["world"];
        EXPECT_EQ(u["xxx"].str(), "99");

        fastring s("{}");
        s.resize(1);
        EXPECT(json::parse(s).is_null());
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
        EXPECT_EQ(v["key"].as_string(), "/\r\n\t\b\f");

        v = json::parse("{ \"key\": \"\u4e2d\u56fd\u4eba\" }");
        fastring s("中国人");
        EXPECT_EQ(v["key"].as_string(), s);
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
