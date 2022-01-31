#include "co/unitest.h"
#include "co/god.h"

namespace test {

DEF_test(god) {
    DEF_case(swap) {
        int v = 1;
        void* p = &v;
        EXPECT_EQ(god::swap(&v, 0), 1);
        EXPECT_EQ(v, 0);
        EXPECT_EQ(god::swap(&p, (void*)0), &v);
        EXPECT_EQ(p, (void*)0);
    }

    DEF_case(b4) {
        EXPECT_EQ(god::b4(0), 0);
        EXPECT_EQ(god::b4(1), 1);
        EXPECT_EQ(god::b4(3), 1);
        EXPECT_EQ(god::b4(15), 4);
        EXPECT_EQ(god::b4(32), 8);
    }

    DEF_case(b8) {
        EXPECT_EQ(god::b8(0), 0);
        EXPECT_EQ(god::b8(1), 1);
        EXPECT_EQ(god::b8(3), 1);
        EXPECT_EQ(god::b8(15), 2);
        EXPECT_EQ(god::b8(32), 4);
    }

    DEF_case(byteseq) {
        if (sizeof(uint32) == 4) {
            const char p[] = "abcdxxxx";
            const char q[] = "abcdyyyy";
            EXPECT(god::byte_eq<uint32>(p, q));
            EXPECT(!god::byte_eq<uint64>(p, q));
        }
    }

    DEF_case(is_same) {
        EXPECT_EQ((god::is_same<char, char>()), true);
        EXPECT_EQ((god::is_same<char, signed char>()), false);
        EXPECT_EQ((god::is_same<char, unsigned char>()), false);
        EXPECT_EQ((god::is_same<int, int&, const int&>()), false);
        EXPECT_EQ((god::is_same<int, char, int, uint32>()), true);
        EXPECT_EQ((god::is_same<int, char, uint32>()), false);
        EXPECT_EQ((god::is_same<void*, const void*>()), false);
        EXPECT_EQ((god::is_same<void*, const void*, void*>()), true);
        EXPECT_EQ((god::is_same<void*, const void*, void**>()), false);
        EXPECT_EQ((god::is_same<void*, std::nullptr_t>()), false);
    }

    DEF_case(is_array) {
        EXPECT_EQ((god::is_array<int[]>()), true);
        EXPECT_EQ((god::is_array<int[][8]>()), true);
        EXPECT_EQ((god::is_array<char[8]>()), true);
        EXPECT_EQ((god::is_array<const char[8]>()), true);
        EXPECT_EQ((god::is_array<const char(&)[8]>()), false);
        EXPECT_EQ((god::is_array<char*>()), false);
    }

    DEF_case(is_pointer) {
        EXPECT_EQ((god::is_pointer<char*>()), true);
        EXPECT_EQ((god::is_pointer<const char*>()), true);
        EXPECT_EQ((god::is_pointer<const char[8]>()), false);
        EXPECT_EQ((god::is_pointer<void*>()), true);
        EXPECT_EQ((god::is_pointer<void**>()), true);
        EXPECT_EQ((god::is_pointer<std::nullptr_t>()), true);
        EXPECT_EQ((god::is_pointer<const std::nullptr_t>()), true);
        EXPECT_EQ((god::is_pointer<const volatile std::nullptr_t>()), true);
        EXPECT_EQ((god::is_pointer<std::string*>()), true);
    }

    DEF_case(is_class) {
        EXPECT_EQ((god::is_class<std::string>()), true);
        EXPECT_EQ((god::is_class<void>()), false);
    }

    DEF_case(type) {
        EXPECT_EQ((god::is_same<god::remove_ref_t<int&>, int>()), true)
        EXPECT_EQ((god::is_same<god::remove_ref_t<const int&>, const int>()), true)
        EXPECT_EQ((god::is_same<god::remove_cv_t<const int&>, const int&>()), true)
        EXPECT_EQ((god::is_same<god::remove_cv_t<const int*>, const int*>()), true)
        EXPECT_EQ((god::is_same<god::remove_cv_t<const int>, int>()), true)
        EXPECT_EQ((god::is_same<god::remove_cv_t<const volatile int>, int>()), true)
        EXPECT_EQ((god::is_same<god::remove_arr_t<int[]>, int>()), true)
        EXPECT_EQ((god::is_same<god::remove_arr_t<int[8]>, int>()), true)
        EXPECT_EQ((god::is_same<god::remove_arr_t<int[][8]>, int[8]>()), true)
        EXPECT_EQ((god::is_same<god::remove_arr_t<char(&)[8]>, char(&)[8]>()), true)
        EXPECT_EQ((god::is_same<god::const_ref_t<int>, const int&>()), true)
        EXPECT_EQ((god::is_same<god::const_ref_t<int&&>, const int&>()), true)
        EXPECT_EQ((god::is_same<god::const_ref_t<const int&>, const int&>()), true)
    }
}

} // test
