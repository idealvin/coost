#include "base/unitest.h"
#include "base/fast.h"

namespace test {

DEF_test(fast) {
    char buf[24];

    DEF_case(u32toa) {
        EXPECT_EQ(fastring(buf, fast::u32toa(0, buf)), "0");
        EXPECT_EQ(fastring(buf, fast::u32toa(1, buf)), "1");
        EXPECT_EQ(fastring(buf, fast::u32toa(12, buf)), "12");
        EXPECT_EQ(fastring(buf, fast::u32toa(123, buf)), "123");
        EXPECT_EQ(fastring(buf, fast::u32toa(1234, buf)), "1234");
        EXPECT_EQ(fastring(buf, fast::u32toa(12345, buf)), "12345");
        EXPECT_EQ(fastring(buf, fast::u32toa(123456, buf)), "123456");
        EXPECT_EQ(fastring(buf, fast::u32toa(1234567, buf)), "1234567");
        EXPECT_EQ(fastring(buf, fast::u32toa(12345678, buf)), "12345678");
        EXPECT_EQ(fastring(buf, fast::u32toa(123456789, buf)), "123456789");
        EXPECT_EQ(fastring(buf, fast::u32toa(1234567890, buf)), "1234567890");
        EXPECT_EQ(fastring(buf, fast::u32toa(3234567890U, buf)), "3234567890");
    }

    DEF_case(i32toa) {
        EXPECT_EQ(fastring(buf, fast::i32toa(0, buf)), "0");
        EXPECT_EQ(fastring(buf, fast::i32toa(1, buf)), "1");
        EXPECT_EQ(fastring(buf, fast::i32toa(-9, buf)), "-9");
        EXPECT_EQ(fastring(buf, fast::i32toa(1234567, buf)), "1234567");
        EXPECT_EQ(fastring(buf, fast::i32toa(-12345678, buf)), "-12345678");
        EXPECT_EQ(fastring(buf, fast::i32toa(-123456789, buf)), "-123456789");
    }

    DEF_case(u64toa) {
        EXPECT_EQ(fastring(buf, fast::u64toa(0, buf)), "0");
        EXPECT_EQ(fastring(buf, fast::u64toa(9, buf)), "9");
        EXPECT_EQ(fastring(buf, fast::u64toa(999, buf)), "999");
        EXPECT_EQ(fastring(buf, fast::u64toa(1234567, buf)), "1234567");
        EXPECT_EQ(fastring(buf, fast::u64toa(12345678, buf)), "12345678");
        EXPECT_EQ(fastring(buf, fast::u64toa(123456789, buf)), "123456789");
        EXPECT_EQ(fastring(buf, fast::u64toa(1234567890, buf)), "1234567890");
        EXPECT_EQ(fastring(buf, fast::u64toa(123456789012ULL, buf)), "123456789012");
        EXPECT_EQ(fastring(buf, fast::u64toa(12345678901234ULL, buf)), "12345678901234");
        EXPECT_EQ(fastring(buf, fast::u64toa(123456789012345ULL, buf)), "123456789012345");
        EXPECT_EQ(fastring(buf, fast::u64toa(1234567890123456ULL, buf)), "1234567890123456");
        EXPECT_EQ(fastring(buf, fast::u64toa(12345678901234567ULL, buf)), "12345678901234567");
        EXPECT_EQ(fastring(buf, fast::u64toa(123456789012345678ULL, buf)), "123456789012345678");
        EXPECT_EQ(fastring(buf, fast::u64toa(1234567890123456789ULL, buf)), "1234567890123456789");
        EXPECT_EQ(fastring(buf, fast::u64toa(12345678901234567890ULL, buf)), "12345678901234567890");
    }

    DEF_case(i64toa) {
        EXPECT_EQ(fastring(buf, fast::i64toa(0, buf)), "0");
        EXPECT_EQ(fastring(buf, fast::i64toa(9, buf)), "9");
        EXPECT_EQ(fastring(buf, fast::i64toa(-9, buf)), "-9");
        EXPECT_EQ(fastring(buf, fast::i64toa(1234567, buf)), "1234567");
        EXPECT_EQ(fastring(buf, fast::i64toa(-12345678, buf)), "-12345678");
        EXPECT_EQ(fastring(buf, fast::i64toa(-123456789, buf)), "-123456789");
        EXPECT_EQ(fastring(buf, fast::i64toa(1234567890, buf)), "1234567890");
        EXPECT_EQ(fastring(buf, fast::i64toa(123456789012ULL, buf)), "123456789012");
        EXPECT_EQ(fastring(buf, fast::i64toa(12345678901234ULL, buf)), "12345678901234");
        EXPECT_EQ(fastring(buf, fast::i64toa(-123456789012345LL, buf)), "-123456789012345");
        EXPECT_EQ(fastring(buf, fast::i64toa(1234567890123456ULL, buf)), "1234567890123456");
        EXPECT_EQ(fastring(buf, fast::i64toa(12345678901234567ULL, buf)), "12345678901234567");
        EXPECT_EQ(fastring(buf, fast::i64toa(123456789012345678ULL, buf)), "123456789012345678");
        EXPECT_EQ(fastring(buf, fast::i64toa(-1234567890123456789LL, buf)), "-1234567890123456789");
    }

    DEF_case(u32toh) {
        EXPECT_EQ(fastring(buf, fast::u32toh(1, buf)), "0x1");
        EXPECT_EQ(fastring(buf, fast::u32toh(0xa, buf)), "0xa");
        EXPECT_EQ(fastring(buf, fast::u32toh(0xf, buf)), "0xf");
        EXPECT_EQ(fastring(buf, fast::u32toh(0xff, buf)), "0xff");
        EXPECT_EQ(fastring(buf, fast::u32toh(0x123456, buf)), "0x123456");
        EXPECT_EQ(fastring(buf, fast::u32toh(0x1234567, buf)), "0x1234567");
        EXPECT_EQ(fastring(buf, fast::u32toh(0x12345678, buf)), "0x12345678");
        EXPECT_EQ(fastring(buf, fast::u32toh(0xffffffff, buf)), "0xffffffff");
    }

    DEF_case(u64toh) {
        EXPECT_EQ(fastring(buf, fast::u64toh(1, buf)), "0x1");
        EXPECT_EQ(fastring(buf, fast::u64toh(0xa, buf)), "0xa");
        EXPECT_EQ(fastring(buf, fast::u64toh(0xf, buf)), "0xf");
        EXPECT_EQ(fastring(buf, fast::u64toh(0xff, buf)), "0xff");
        EXPECT_EQ(fastring(buf, fast::u64toh(0x123456, buf)), "0x123456");
        EXPECT_EQ(fastring(buf, fast::u64toh(0x1234567, buf)), "0x1234567");
        EXPECT_EQ(fastring(buf, fast::u64toh(0x12345678, buf)), "0x12345678");
        EXPECT_EQ(fastring(buf, fast::u64toh(0xffffffff, buf)), "0xffffffff");
        EXPECT_EQ(fastring(buf, fast::u64toh(0xfffffffffULL, buf)), "0xfffffffff");
        EXPECT_EQ(fastring(buf, fast::u64toh(0xffffffffffffULL, buf)), "0xffffffffffff");
        EXPECT_EQ(fastring(buf, fast::u64toh(0xffffffffffffffffULL, buf)), "0xffffffffffffffff");
        EXPECT_EQ(fastring(buf, fast::u64toh(0x1234567890ULL, buf)), "0x1234567890");
        EXPECT_EQ(fastring(buf, fast::u64toh(0x123456789abcdefULL, buf)), "0x123456789abcdef");
    }

    DEF_case(dtoa) {
        EXPECT_EQ(fastring(buf, fast::dtoa(0.0, buf)), "0");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.01, buf)), "0.01");
        EXPECT_EQ(fastring(buf, fast::dtoa(-0.1, buf)), "-0.1");
        EXPECT_EQ(fastring(buf, fast::dtoa(3.14, buf)), "3.14");
        EXPECT_EQ(fastring(buf, fast::dtoa(3.14159, buf)), "3.14159");
    }
}

} // namespace test
