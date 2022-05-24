#include "co/unitest.h"
#include "co/hash.h"
#include "co/time.h"
#include "co/str.h"

namespace test {

DEF_test(hash) {
    DEF_case(base64) {
        EXPECT_EQ(base64_encode(""), "");
        EXPECT_EQ(base64_decode(""), "");
        EXPECT_EQ(base64_encode("hello world"), "aGVsbG8gd29ybGQ=");
        EXPECT_EQ(base64_decode("aGVsbG8gd29ybGQ="), "hello world");
        EXPECT_EQ(base64_decode("aGVsbG8gd29ybGQ=\r\n"), "hello world");
        EXPECT_EQ(base64_decode("aGVsbG8gd29ybGQ\r\n="), "");
        EXPECT_EQ(base64_decode("aGVs\nbG8gd29ybGQ="), "");
        EXPECT_EQ(base64_decode("aGVs\r\nbG8gd29ybGQ="), "hello world");

        auto s = str::from(now::us());
        EXPECT_EQ(base64_decode(base64_encode(s)), s);
    }

    DEF_case(md5sum) {
        EXPECT_EQ(md5sum(""), "d41d8cd98f00b204e9800998ecf8427e");
        EXPECT_EQ(md5sum("hello world"), "5eb63bbbe01eeed093cb22bb8f5acdc3");
    }

    DEF_case(url_code) {
        EXPECT_EQ(
            url_encode("https://github.com/idealvin/co/xx.cc#L23"),
            "https://github.com/idealvin/co/xx.cc#L23"
        );

        EXPECT_EQ(
            url_encode("http://xx.com/hello world"),
            "http://xx.com/hello%20world"
        );

        EXPECT_EQ(
            url_decode("http://xx.com/hello%20world"),
            "http://xx.com/hello world"
        );
    }

    DEF_case(hash) {
        if (sizeof(void*) == 8) {
            EXPECT_EQ(hash64("hello"), murmur_hash("hello", 5));
        } else {
            EXPECT_EQ(hash32("hello"), murmur_hash("hello", 5));
        }

        EXPECT_NE(crc16("hello"), 0);
    }
}

} // test
