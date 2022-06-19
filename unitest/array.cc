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

    DEF_case(construct_with_cap) {
        co::array<int> v(8);
        EXPECT_EQ(v.size(), 0);
        EXPECT_EQ(v.capacity(), 8);
    }

    DEF_case(construct) {
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
    }

    DEF_case(copy) {
        co::array<int> v = { 1, 2, 3 };
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], 1);
        EXPECT_EQ(v[1], 2);
        EXPECT_EQ(v[2], 3);

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

    DEF_case(modify) {
        int x[4] = { 1, 2, 3, 4 };
        co::array<int> v(x, 4);
        EXPECT_EQ(v.size(), 4);
        EXPECT_EQ(v[0], 1);
        EXPECT_EQ(v[1], 2);
        EXPECT_EQ(v[2], 3);
        EXPECT_EQ(v[3], 4);

        v.resize(2);
        EXPECT_EQ(v.size(), 2);
        EXPECT_EQ(v[1], 2);

        v.push_back(7);
        v.push_back(8);
        EXPECT_EQ(v.size(), 4);
        EXPECT_EQ(v[3], 8);

        v.push_back(10, 6);
        EXPECT_EQ(v.size(), 14);
        EXPECT_EQ(v[4], 6);
        EXPECT_EQ(v[13], 6);

        v.remove_back();
        EXPECT_EQ(v.size(), 13);

        EXPECT_EQ(v.pop_back(), 6);
        EXPECT_EQ(v.size(), 12);

        v.resize(4);
        v.remove(1);
        EXPECT_EQ(v.size(), 3);
        EXPECT_EQ(v[0], 1);
        EXPECT_EQ(v[1], 8);
        EXPECT_EQ(v[2], 7);

        EXPECT_EQ(*v.begin(), 1);
    }
}

}
