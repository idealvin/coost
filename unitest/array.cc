#include "co/unitest.h"
#include "co/array.h"

namespace test {

DEF_test(array){
    DEF_case(base) {
        co::array<int> v;
        EXPECT(v.empty());
        EXPECT_EQ(v.size(), 0);
        EXPECT_EQ(v.capacity(), 0);
        EXPECT_EQ(v.data(), (int*)0);

        v.reserve(32);
        EXPECT_EQ(v.capacity(), 32);
        v.reserve(16);
        EXPECT_EQ(v.capacity(), 32);
    }

    DEF_case(construct) {
        co::array<int> _(8);
        EXPECT_EQ(_.size(), 0);
        EXPECT_EQ(_.capacity(), 8);

        co::array<int> v(8, 7);
        EXPECT_EQ(v.size(), 8);
        EXPECT_EQ(v.capacity(), 8);
        EXPECT_EQ(v[0], 7);
        EXPECT_EQ(v[7], 7);
        EXPECT_EQ(v.front(), 7);
        EXPECT_EQ(v.back(), 7);

        co::array<int> u(v);
        EXPECT_EQ(u.size(), 8);
        EXPECT_EQ(u.capacity(), 8);
        EXPECT_EQ(u[0], 7);
        EXPECT_EQ(u[7], 7);

        u.clear();
        EXPECT_EQ(u.size(), 0);
        EXPECT_EQ(u.capacity(), 8);

        u.reset();
        EXPECT_EQ(u.capacity(), 0);

        u.swap(v);
        EXPECT_EQ(v.size(), 0);
        EXPECT_EQ(v.capacity(), 0);
        EXPECT_EQ(u.size(), 8);
        EXPECT_EQ(u.capacity(), 8);

        co::array<int> w(std::move(u));
        EXPECT_EQ(u.size(), 0);
        EXPECT_EQ(u.capacity(), 0);
        EXPECT_EQ(w.size(), 8);
        EXPECT_EQ(w.capacity(), 8);

        co::array<fastring> a(8, 0);
        EXPECT_EQ(a.size(), 8);
        EXPECT_EQ(a.capacity(), 8);
        EXPECT_EQ(a[0].data(), (const char*)0);

        co::array<fastring> b(8, (size_t)16);
        EXPECT_EQ(b.size(), 8);
        EXPECT_EQ(b.capacity(), 8);
        EXPECT_EQ(b[0].capacity(), 16);
        EXPECT_EQ(b[7].capacity(), 16);

        co::array<int> x = { 1, 2, 3 };
        EXPECT_EQ(x.size(), 3);
        EXPECT_EQ(x[0], 1);
        EXPECT_EQ(x[1], 2);
        EXPECT_EQ(x[2], 3);

        co::array<int> z(x.begin(), x.end());
        EXPECT_EQ(z.size(), 3);
        EXPECT_EQ(z[0], 1);
        EXPECT_EQ(z[1], 2);
        EXPECT_EQ(z[2], 3);

        co::array<int> k(z.data(), 2);
        EXPECT_EQ(k.size(), 2);
        EXPECT_EQ(k[0], 1);
        EXPECT_EQ(k[1], 2);

        int xx[4] = { 1, 2, 3, 4 };
        co::array<int> m(xx, 4);
        EXPECT_EQ(m.size(), 4);
        EXPECT_EQ(m[0], 1);
        EXPECT_EQ(m[3], 4);
    }

    DEF_case(copy) {
        co::array<int> v = { 1, 2, 3 };
        co::array<int> u(++v.begin(), v.end());
        EXPECT_EQ(u.size(), 2);
        EXPECT_EQ(u[0], 2);
        EXPECT_EQ(u[1], 3);

        u = v;
        EXPECT_EQ(u.size(), 3);
        EXPECT_EQ(u[0], 1);

        u = { 7, 8, 9, 9 };
        EXPECT_EQ(u.size(), 4);
        EXPECT_EQ(u[0], 7);
        EXPECT_EQ(u[3], 9);

        u = std::move(v);
        EXPECT_EQ(v.size(), 0);
        EXPECT_EQ(v.capacity(), 0);
        EXPECT_EQ(u.size(), 3);
        EXPECT_EQ(u[0], 1);
        EXPECT_EQ(u[2], 3);
    }

    DEF_case(append) {
        co::array<int> v(4);
        v.append(v);
        EXPECT_EQ(v.size(), 0);

        v.append(std::move(v));
        EXPECT_EQ(v.size(), 0);

        v.append(1);
        EXPECT_EQ(v.size(), 1);
        EXPECT_EQ(v[0], 1);

        v.append(3, 8);
        EXPECT_EQ(v.size(), 4);
        EXPECT_EQ(v[1], 8);
        EXPECT_EQ(v[3], 8);

        v.append(v);
        EXPECT_EQ(v.size(), 8);
        EXPECT_EQ(v[4], 1);
        EXPECT_EQ(v[5], 8);
        EXPECT_EQ(v[7], 8);

        v.resize(4);
        v.append(std::move(v));
        EXPECT_EQ(v.size(), 8);
        EXPECT_EQ(v[4], 1);
        EXPECT_EQ(v[5], 8);
        EXPECT_EQ(v[7], 8);

        int x[4] = { 7, 7, 7, 7 };
        v.resize(4);
        v.append(x, 4);
        EXPECT_EQ(v.size(), 8);
        EXPECT_EQ(v[4], 7);
        EXPECT_EQ(v[7], 7);

        v.safe_append(v.data() + 4, 4);
        EXPECT_EQ(v.size(), 12);
        EXPECT_EQ(v[8], 7);
        EXPECT_EQ(v[11], 7);

        fastring s("hello");
        const auto p = s.data();
        co::array<fastring> u;
        u.append(std::move(s));
        EXPECT_EQ(u[0].data(), p);

        co::array<fastring> w;
        w.append(std::move(u));
        EXPECT_EQ(w[0].data(), p);
        EXPECT_EQ(u[0].size(), 0);
        EXPECT_EQ(u[0].capacity(), 0);
    }

    DEF_case(modify) {
        co::array<int> v = { 1, 2, 3, 4 };
        EXPECT_EQ(v.size(), 4);

        v.resize(2);
        EXPECT_EQ(v.size(), 2);
        EXPECT_EQ(v[1], 2);

        v.push_back(7);
        v.push_back(8);
        EXPECT_EQ(v.size(), 4);
        EXPECT_EQ(v[3], 8);

        v.remove_back();
        EXPECT_EQ(v.size(), 3);

        EXPECT_EQ(v.pop_back(), 7);
        EXPECT_EQ(v.size(), 2);

        v.push_back(3);
        v.push_back(4);
        v.remove(1);
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], 1);
        EXPECT_EQ(v[1], 4);
        EXPECT_EQ(v[2], 3);
        EXPECT_EQ(*v.begin(), 1);

        fastring s("hello");
        const auto p = s.data();
        co::array<fastring> a(8);
        a.push_back(std::move(s));
        EXPECT_EQ(a.size(), 1);
        EXPECT_EQ(a[0].data(), p);

        fastring t = a.pop_back();
        EXPECT(a.empty());
        EXPECT_EQ(t.data(), p);

        a.emplace(3, 'x');
        EXPECT_EQ(a.size(), 1);
        EXPECT_EQ(a[0], "xxx");

        a.emplace("8888");
        EXPECT_EQ(a.size(), 2);
        EXPECT_EQ(a.back(), "8888");
    }

    static int gc = 0;
    static int gd = 0;

    struct A {
        A() { ++gc; }
        A(const A&) { ++gc; }
        A(A&&) { ++gc; }
        ~A() { ++gd; }
    };

    DEF_case(resize) {
        co::array<int> _(8, 3);
        _.resize(16);
        EXPECT_EQ(_[7], 3);
        EXPECT_EQ(_[8], 0);
        EXPECT_EQ(_[15], 0);

        co::array<A> a(8, 0);
        EXPECT_EQ(gc, 8);

        a.resize(16);
        EXPECT_EQ(gc, 16);

        a.resize(8);
        EXPECT_EQ(gd, 8);

        co::array<A> b(a);
        EXPECT_EQ(gc, 24);

        co::array<A> c(std::move(b));
        EXPECT_EQ(gc, 24);

        c.remove_back();
        EXPECT_EQ(gd, 9);

        c.remove(3);
        EXPECT_EQ(gc, 25);
        EXPECT_EQ(gd, 11);

        a.reset();
        b.reset();
        c.reset();
        EXPECT_EQ(gd, 25);
        EXPECT_EQ(gc, gd);
    }
}

}
