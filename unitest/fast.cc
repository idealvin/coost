#include "co/unitest.h"
#include "co/fast.h"

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

    DEF_case(i32toa) {
        EXPECT_EQ(fastring(buf, fast::i32toa(0, buf)), "0");
        EXPECT_EQ(fastring(buf, fast::i32toa(1, buf)), "1");
        EXPECT_EQ(fastring(buf, fast::i32toa(-9, buf)), "-9");
        EXPECT_EQ(fastring(buf, fast::i32toa(1234567, buf)), "1234567");
        EXPECT_EQ(fastring(buf, fast::i32toa(-12345678, buf)), "-12345678");
        EXPECT_EQ(fastring(buf, fast::i32toa(-123456789, buf)), "-123456789");
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

    DEF_case(itoa) {
        EXPECT_EQ(fastring(buf, fast::itoa(0, buf)), "0");
        EXPECT_EQ(fastring(buf, fast::itoa(-12345678, buf)), "-12345678");
        EXPECT_EQ(fastring(buf, fast::itoa(-123456789012345678LL, buf)), "-123456789012345678");
    }

    DEF_case(utoa) {
        EXPECT_EQ(fastring(buf, fast::itoa(0U, buf)), "0");
        EXPECT_EQ(fastring(buf, fast::itoa(123456789U, buf)), "123456789");
        EXPECT_EQ(fastring(buf, fast::itoa(1234567890123456789ULL, buf)), "1234567890123456789");
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
        EXPECT_EQ(fastring(buf, fast::u64toh(0x1234567890abcdefULL, buf)), "0x1234567890abcdef");
    }

    DEF_case(ptoh) {
        EXPECT_EQ(fastring(buf, fast::ptoh((void*)0x123456, buf)), "0x123456");
        EXPECT_EQ(fastring(buf, fast::ptoh((void*)0x12345678, buf)), "0x12345678");
        if (sizeof(void*) >= sizeof(uint64)) {
            EXPECT_EQ(fastring(buf, fast::ptoh((void*)(size_t)0x1234567890abcdefULL, buf)), "0x1234567890abcdef");
        }
    }

    DEF_case(dtoa) {
        EXPECT_EQ(fastring(buf, fast::dtoa(0.0, buf)), "0.0");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.00, buf)), "0.0");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.01, buf)), "0.01");
        EXPECT_EQ(fastring(buf, fast::dtoa(-0.1, buf)), "-0.1");
        EXPECT_EQ(fastring(buf, fast::dtoa(3.14, buf)), "3.14");
        EXPECT_EQ(fastring(buf, fast::dtoa(3.14159, buf)), "3.14159");
        EXPECT_EQ(fastring(buf, fast::dtoa(3e-23, buf)), "3e-23");
        EXPECT_EQ(fastring(buf, fast::dtoa(3.33e-23, buf)), "3.33e-23");
        EXPECT_EQ(fastring(buf, fast::dtoa(3e23, buf)), "3e23");
        EXPECT_EQ(fastring(buf, fast::dtoa(1e30, buf)), "1e30");
        EXPECT_EQ(fastring(buf, fast::dtoa(3.33e23, buf)), "3.33e23");
        EXPECT_EQ(fastring(buf, fast::dtoa(1.79e308, buf)), "1.79e308");
        EXPECT_EQ(fastring(buf, fast::dtoa(2.23e-308, buf)), "2.23e-308");
        EXPECT_EQ(fastring(buf, fast::dtoa(5e-324, buf)), "5e-324");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.001, buf)), "0.001");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.00123456, buf)), "0.00123456");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.0001, buf)), "1e-4");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.000001, buf)), "1e-6");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.0000001, buf)), "1e-7");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.123456789012, buf)), "0.123456789012");
        EXPECT_EQ(fastring(buf, fast::dtoa(-79.39773355813419, buf)), "-79.39773355813419");
        EXPECT_EQ(fastring(buf, fast::dtoa(1.234567890123456e30, buf)), "1.234567890123456e30");
        EXPECT_EQ(fastring(buf, fast::dtoa(2.225073858507201e-308, buf)), "2.225073858507201e-308");
        EXPECT_EQ(fastring(buf, fast::dtoa(2.2250738585072014e-308, buf)), "2.2250738585072014e-308");
        EXPECT_EQ(fastring(buf, fast::dtoa(1.7976931348623157e308, buf)), "1.7976931348623157e308");

        EXPECT_EQ(fastring(buf, fast::dtoa(0.123456, buf, 6)), "0.123456");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.123456, buf, 3)), "0.123");
        EXPECT_EQ(fastring(buf, fast::dtoa(-0.123456, buf, 6)), "-0.123456");
        EXPECT_EQ(fastring(buf, fast::dtoa(-0.123456, buf, 3)), "-0.123");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.0123456, buf, 7)), "0.0123456");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.0123456, buf, 6)), "1.23456e-2");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.0123456, buf, 3)), "1.234e-2");
        EXPECT_EQ(fastring(buf, fast::dtoa(-0.0123456, buf, 7)), "-0.0123456");
        EXPECT_EQ(fastring(buf, fast::dtoa(-0.0123456, buf, 3)), "-1.234e-2");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.00123456, buf, 8)), "0.00123456");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.00123456, buf, 3)), "1.234e-3");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.000123456, buf, 9)), "1.23456e-4");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.000123456, buf, 3)), "1.234e-4");
        EXPECT_EQ(fastring(buf, fast::dtoa(-0.0000123, buf, 8)), "-1.23e-5");
        EXPECT_EQ(fastring(buf, fast::dtoa(-0.0000123, buf, 3)), "-1.23e-5");
        EXPECT_EQ(fastring(buf, fast::dtoa(1.234567, buf, 6)), "1.234567");
        EXPECT_EQ(fastring(buf, fast::dtoa(1.234567, buf, 3)), "1.234");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.1000234567, buf, 10)), "0.1000234567");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.1000234567, buf, 5)), "0.10002");
        EXPECT_EQ(fastring(buf, fast::dtoa(0.1000234567, buf, 3)), "0.1");
        EXPECT_EQ(fastring(buf, fast::dtoa(1.1000234567, buf, 10)), "1.1000234567");
        EXPECT_EQ(fastring(buf, fast::dtoa(1.1000234567, buf, 3)), "1.1");
        EXPECT_EQ(fastring(buf, fast::dtoa(12345e3, buf, 6)), "12345000.0");
        EXPECT_EQ(fastring(buf, fast::dtoa(123456789012345678901234.0, buf, 6)), "1.234567e23");
        EXPECT_EQ(fastring(buf, fast::dtoa(1.23456789e-9, buf, 5)), "1.23456e-9");
        EXPECT_EQ(fastring(buf, fast::dtoa(1.23456789e-9, buf, 2)), "1.23e-9");
        EXPECT_EQ(fastring(buf, fast::dtoa(123000e30, buf, 2)), "1.23e35");
        EXPECT_EQ(fastring(buf, fast::dtoa(123000e30, buf, 1)), "1.2e35");
        EXPECT_EQ(fastring(buf, fast::dtoa(12345e-8, buf, 4)), "1.2345e-4");
        EXPECT_EQ(fastring(buf, fast::dtoa(12345e-8, buf, 2)), "1.23e-4");
    }
}

} // namespace test
