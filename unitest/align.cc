#include "co/unitest.h"
#include "co/align.h"

namespace test {

template<uint32 N, uint32 Bits>
constexpr uint32 nb(uint32 x) noexcept {
    static_assert(N == (1u << Bits));
    return (x >> Bits) + !!(x & (N - 1));
}

DEF_test(align) {
    DEF_case(align) {
        static_assert(co::align_up<8>(0) == 0);
        static_assert(co::align_up<8>(30) == 32);
        static_assert(co::align_up(30, 8) == 32);

        EXPECT_EQ(co::align_up<8>(30), 32);
        EXPECT_EQ(co::align_up<64>(30), 64);
        EXPECT_EQ(co::align_down<8>(30), 24);
        EXPECT_EQ(co::align_down<64>(30), 0);
        EXPECT_EQ(co::align_up(30, 8), 32);
        EXPECT_EQ(co::align_up(61, 32), 64);
        EXPECT_EQ(co::align_down(30, 8), 24);

        void* p = (void*)4000;
        EXPECT_EQ(co::align_up<4096>(p), (void*)4096);
        EXPECT_EQ(co::align_up(p, 4096), (void*)4096);
        EXPECT_EQ(co::align_down<4096>(p), (void*)0);
        EXPECT_EQ(co::align_down(p, 4096), (void*)0);
    }

    DEF_case(nb) {
        EXPECT_EQ((nb<16, 4>(32)), 2)
        EXPECT_EQ((nb<16, 4>(32)), 2)
        EXPECT_EQ((nb<16, 4>(33)), 3)
        EXPECT_EQ((nb<4096, 12>(123)), 1)
        EXPECT_EQ((nb<4096, 12>(4097)), 2)
    }
}

} // test
