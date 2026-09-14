#include "co/stl.h"
#include "co/unitest.h"

namespace test {

DEF_test(stl) {
    co::string a("hello");
    const char b[] = "hello";

    DEF_case(compare) {
        auto lt = co::less<const char*>();
        auto gt = co::greater<const char*>();
        auto eq = co::xx::eq<const char*>();
        EXPECT(!lt(a.c_str(), b));
        EXPECT(!lt(b, a.c_str()));
        EXPECT(!gt(a.c_str(), b));
        EXPECT(!gt(b, a.c_str()));
        EXPECT(eq(a.c_str(), b));
        EXPECT(a.c_str() != b);
    }

    DEF_case(hash) {
        auto h = co::hash<const char*>();
        EXPECT_NE(h(a.c_str()), (size_t)a.c_str());
        EXPECT_NE(h(b), (size_t)b);
        EXPECT_EQ(h(a.c_str()), h(b));
    }

    DEF_case(priority_queue) {
        co::priority_queue<int> qmax;
        for (int i = 0; i < 4; ++i) {
            qmax.push(i);
        }
        EXPECT_EQ(qmax.top(), 3);
        qmax.pop();
        EXPECT_EQ(qmax.top(), 2);

        co::priority_queue<int, co::greater<int>> qmin; 
        for (int i = 0; i < 4; ++i) {
            qmin.push(i);
        }
        EXPECT_EQ(qmin.top(), 0);
        qmin.pop();
        EXPECT_EQ(qmin.top(), 1);
    }

    DEF_case(map) {
        co::map<const char*, int> m;
        m[a.c_str()] = 1;
        EXPECT(m.find(b) != m.end());
        EXPECT(m[b] == 1);
        auto r = m.emplace(b, 2);
        EXPECT(!r.second);
        EXPECT(m[b] == 1);
    }

    DEF_case(hash_set) {
        co::hash_set<const char*> s;
        s.insert(a.c_str());
        auto r = s.insert(b);
        EXPECT(!r.second);
        EXPECT(s.size() == 1);
        EXPECT(s.find(b) != s.end());
    }

    DEF_case(format) {
        co::vector<int> v { 1, 2, 3 };
        co::string s(32);
        s << v;
        EXPECT_EQ(s, "[1,2,3]");

        co::map<int, int> m {
            {1, 1},
            {2, 2},
            {3, 3},
        };
        s.clear();
        s << m;
        EXPECT_EQ(s, "{1:1,2:2,3:3}")

        co::set<int> x;
        x.insert(2);
        x.insert(3);
        x.insert(1);
        s.clear();
        s << x;
        EXPECT_EQ(s, "{1,2,3}");
    }
}

} // test
