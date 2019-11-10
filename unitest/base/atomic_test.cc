#include "base/unitest.h"
#include "base/def.h"
#include "base/atomic.h"

namespace test {

DEF_test(atomic) {
    bool b = false;
    int8 i8 = 0;
    int16 i16 = 0;
    int32 i32 = 0;
    int64 i64 = 0;

    DEF_case(inc) {
        EXPECT_EQ(atomic_inc(&i8), 1);
        EXPECT_EQ(atomic_inc(&i16), 1);
        EXPECT_EQ(atomic_inc(&i32), 1);
        EXPECT_EQ(atomic_inc(&i64), 1);
    }

    DEF_case(dec) {
        EXPECT_EQ(atomic_dec(&i8), 0);
        EXPECT_EQ(atomic_dec(&i16), 0);
        EXPECT_EQ(atomic_dec(&i32), 0);
        EXPECT_EQ(atomic_dec(&i64), 0);
    }

    DEF_case(add) {
        EXPECT_EQ(atomic_add(&i8, 1), 1);
        EXPECT_EQ(atomic_add(&i16, 1), 1);
        EXPECT_EQ(atomic_add(&i32, 1), 1);
        EXPECT_EQ(atomic_add(&i64, 1), 1);
    }

    DEF_case(sub) {
        EXPECT_EQ(atomic_sub(&i8, 1), 0);
        EXPECT_EQ(atomic_sub(&i16, 1), 0);
        EXPECT_EQ(atomic_sub(&i32, 1), 0);
        EXPECT_EQ(atomic_sub(&i64, 1), 0);
    }

    DEF_case(fetch_inc) {
        EXPECT_EQ(atomic_fetch_inc(&i8), 0);
        EXPECT_EQ(atomic_fetch_inc(&i16), 0);
        EXPECT_EQ(atomic_fetch_inc(&i32), 0);
        EXPECT_EQ(atomic_fetch_inc(&i64), 0);
    }

    DEF_case(fetch_dec) {
        EXPECT_EQ(atomic_fetch_dec(&i8), 1);
        EXPECT_EQ(atomic_fetch_dec(&i16), 1);
        EXPECT_EQ(atomic_fetch_dec(&i32), 1);
        EXPECT_EQ(atomic_fetch_dec(&i64), 1);
    }

    DEF_case(fetch_add) {
        EXPECT_EQ(atomic_fetch_add(&i8, 1), 0);
        EXPECT_EQ(atomic_fetch_add(&i16, 1), 0);
        EXPECT_EQ(atomic_fetch_add(&i32, 1), 0);
        EXPECT_EQ(atomic_fetch_add(&i64, 1), 0);
    }

    DEF_case(fetch_sub) {
        EXPECT_EQ(atomic_fetch_sub(&i8, 1), 1);
        EXPECT_EQ(atomic_fetch_sub(&i16, 1), 1);
        EXPECT_EQ(atomic_fetch_sub(&i32, 1), 1);
        EXPECT_EQ(atomic_fetch_sub(&i64, 1), 1);
    }

    DEF_case(or) {
        EXPECT_EQ(atomic_or(&i8, 1), 1);
        EXPECT_EQ(atomic_or(&i16, 1), 1);
        EXPECT_EQ(atomic_or(&i32, 1), 1);
        EXPECT_EQ(atomic_or(&i64, 1), 1);
    }

    DEF_case(xor) {
        EXPECT_EQ(atomic_xor(&i8, 3), 2);
        EXPECT_EQ(atomic_xor(&i16, 3), 2);
        EXPECT_EQ(atomic_xor(&i32, 3), 2);
        EXPECT_EQ(atomic_xor(&i64, 3), 2);
    }

    DEF_case(and) {
        EXPECT_EQ(atomic_and(&i8, 1), 0);
        EXPECT_EQ(atomic_and(&i16, 1), 0);
        EXPECT_EQ(atomic_and(&i32, 1), 0);
        EXPECT_EQ(atomic_and(&i64, 1), 0);
    }

    DEF_case(fetch_or) {
        EXPECT_EQ(atomic_fetch_or(&i8, 1), 0);
        EXPECT_EQ(atomic_fetch_or(&i16, 1), 0);
        EXPECT_EQ(atomic_fetch_or(&i32, 1), 0);
        EXPECT_EQ(atomic_fetch_or(&i64, 1), 0);
    }

    DEF_case(fetch_xor) {
        EXPECT_EQ(atomic_fetch_xor(&i8, 3), 1);
        EXPECT_EQ(atomic_fetch_xor(&i16, 3), 1);
        EXPECT_EQ(atomic_fetch_xor(&i32, 3), 1);
        EXPECT_EQ(atomic_fetch_xor(&i64, 3), 1);
    }

    DEF_case(fetch_and) {
        EXPECT_EQ(atomic_fetch_and(&i8, 1), 2);
        EXPECT_EQ(atomic_fetch_and(&i16, 1), 2);
        EXPECT_EQ(atomic_fetch_and(&i32, 1), 2);
        EXPECT_EQ(atomic_fetch_and(&i64, 1), 2);
    }

    DEF_case(swap) {
        EXPECT_EQ(atomic_swap(&b, true), false);
        EXPECT_EQ(atomic_swap(&i8, 1), 0);
        EXPECT_EQ(atomic_swap(&i16, 1), 0);
        EXPECT_EQ(atomic_swap(&i32, 1), 0);
        EXPECT_EQ(atomic_swap(&i64, 1), 0);
    }

    DEF_case(compare_swap) {
        EXPECT_EQ(atomic_compare_swap(&b, true, false), true);
        EXPECT_EQ(atomic_compare_swap(&i8, 1, 0), 1);
        EXPECT_EQ(atomic_compare_swap(&i16, 1, 0), 1);
        EXPECT_EQ(atomic_compare_swap(&i32, 1, 0), 1);
        EXPECT_EQ(atomic_compare_swap(&i64, 1, 0), 1);
    }

    DEF_case(get) {
        EXPECT_EQ(atomic_get(&b), false);
        EXPECT_EQ(atomic_get(&i8), 0);
        EXPECT_EQ(atomic_get(&i16), 0);
        EXPECT_EQ(atomic_get(&i32), 0);
        EXPECT_EQ(atomic_get(&i64), 0);
    }

    DEF_case(set) {
        atomic_set(&b, true);
        atomic_set(&i8, 1);
        atomic_set(&i16, 1);
        atomic_set(&i32, 1);
        atomic_set(&i64, 1);
        EXPECT_EQ(atomic_get(&b), true);
        EXPECT_EQ(atomic_get(&i8), 1);
        EXPECT_EQ(atomic_get(&i16), 1);
        EXPECT_EQ(atomic_get(&i32), 1);
        EXPECT_EQ(atomic_get(&i64), 1);
    }

    DEF_case(reset) {
        atomic_reset(&b);
        atomic_reset(&i8);
        atomic_reset(&i16);
        atomic_reset(&i32);
        atomic_reset(&i64);
        EXPECT_EQ(atomic_get(&b), false);
        EXPECT_EQ(atomic_get(&i8), 0);
        EXPECT_EQ(atomic_get(&i16), 0);
        EXPECT_EQ(atomic_get(&i32), 0);
        EXPECT_EQ(atomic_get(&i64), 0);
    }
}

} // namespace test
