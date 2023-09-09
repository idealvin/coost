#include "co/unitest.h"
#include "co/god.h"

namespace test {

DEF_test(god) {
    god::bless_no_bugs();

    DEF_case(cast) {
        EXPECT_EQ(god::cast<int>(false), 0);
        EXPECT_EQ(god::cast<int>(1.007), 1);
        static_assert(god::cast<int>(3.2) == 3, "");

        fastring s("hello");
        fastream& x = god::cast<fastream&>(s);
        x << " world";
        EXPECT_EQ(s, "hello world");
    }

    DEF_case(align) {
        EXPECT_EQ(god::align_up<8>(30), 32);
        EXPECT_EQ(god::align_up<64>(30), 64);
        EXPECT_EQ(god::align_down<8>(30), 24);
        EXPECT_EQ(god::align_down<64>(30), 0);
        EXPECT_EQ(god::align_up(30, 8), 32);
        EXPECT_EQ(god::align_up(61, 32), 64);
        EXPECT_EQ(god::align_down(30, 8), 24);

        void* p = (void*)4000;
        EXPECT_EQ(god::align_up<4096>(p), (void*)4096);
        EXPECT_EQ(god::align_up(p, 4096), (void*)4096);
        EXPECT_EQ(god::align_down<4096>(p), (void*)0);
        EXPECT_EQ(god::align_down(p, 4096), (void*)0);
    }

    DEF_case(nb) {
        EXPECT_EQ(god::nb<4>(0), 0);
        EXPECT_EQ(god::nb<4>(1), 1);
        EXPECT_EQ(god::nb<4>(3), 1);
        EXPECT_EQ(god::nb<4>(15), 4);
        EXPECT_EQ(god::nb<4>(32), 8);
        EXPECT_EQ(god::nb<8>(0), 0);
        EXPECT_EQ(god::nb<8>(1), 1);
        EXPECT_EQ(god::nb<8>(3), 1);
        EXPECT_EQ(god::nb<8>(15), 2);
        EXPECT_EQ(god::nb<8>(32), 4);
    }

    DEF_case(eq) {
        const char p[] = "abcdxxxx";
        const char q[] = "abcdyyyy";
        EXPECT(god::eq<uint32>(p, q));
        EXPECT(!god::eq<uint64>(p, q));
    }

    DEF_case(copy) {
        fastring s("1234567");
        fastring t("hello");
        god::copy<3>(&s[2], &t[1]);
        EXPECT_EQ(s, "12ell67");
    }

    DEF_case(swap) {
        int v = 1;
        void* p = &v;
        EXPECT_EQ(god::swap(&v, 0), 1);
        EXPECT_EQ(v, 0);
        EXPECT_EQ(god::swap(&p, (void*)0), &v);
        EXPECT_EQ(p, (void*)0);
    }

    DEF_case(fetch_xx) {
        int v = 0;
        EXPECT_EQ(god::fetch_add(&v, 7), 0);
        EXPECT_EQ(v, 7);
        EXPECT_EQ(god::fetch_sub(&v, 3), 7);
        EXPECT_EQ(v, 4);
        EXPECT_EQ(god::fetch_and(&v, 5), 4);
        EXPECT_EQ(v, 4);
        EXPECT_EQ(god::fetch_or(&v, 3), 4);
        EXPECT_EQ(v, 7);
        EXPECT_EQ(god::fetch_xor(&v, 5), 7);
        EXPECT_EQ(v, 2);
    }

    DEF_case(type) {
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

        EXPECT_EQ((god::is_same<god::rm_ref_t<int&>, int>()), true)
        EXPECT_EQ((god::is_same<god::rm_ref_t<const int&>, const int>()), true)
        EXPECT_EQ((god::is_same<god::rm_cv_t<const int&>, const int&>()), true)
        EXPECT_EQ((god::is_same<god::rm_cv_t<const int*>, const int*>()), true)
        EXPECT_EQ((god::is_same<god::rm_cv_t<const int>, int>()), true)
        EXPECT_EQ((god::is_same<god::rm_cv_t<const volatile int>, int>()), true)
        EXPECT_EQ((god::is_same<god::rm_cvref_t<const volatile int&>, int>()), true)
        EXPECT_EQ((god::is_same<god::rm_arr_t<int[]>, int>()), true)
        EXPECT_EQ((god::is_same<god::rm_arr_t<int[8]>, int>()), true)
        EXPECT_EQ((god::is_same<god::rm_arr_t<int[][8]>, int[8]>()), true)
        EXPECT_EQ((god::is_same<god::rm_arr_t<char(&)[8]>, char(&)[8]>()), true)
        EXPECT_EQ((god::is_same<god::const_ref_t<int>, const int&>()), true)
        EXPECT_EQ((god::is_same<god::const_ref_t<int&&>, const int&>()), true)
        EXPECT_EQ((god::is_same<god::const_ref_t<const int&>, const int&>()), true)

        EXPECT_EQ((god::is_ref<int&>()), true);
        EXPECT_EQ((god::is_ref<int&&>()), true);
        EXPECT_EQ((god::is_ref<int>()), false);

        EXPECT_EQ((god::is_array<int[]>()), true);
        EXPECT_EQ((god::is_array<int[][8]>()), true);
        EXPECT_EQ((god::is_array<char[8]>()), true);
        EXPECT_EQ((god::is_array<const char[8]>()), true);
        EXPECT_EQ((god::is_array<const char(&)[8]>()), false);
        EXPECT_EQ((god::is_array<char*>()), false);

        EXPECT_EQ((god::is_class<std::string>()), true);
        EXPECT_EQ((god::is_class<void>()), false);
    }
}

} // test
