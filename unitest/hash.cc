#include "co/unitest.h"
#include "co/base64.h"
#include "co/md5.h"
#include "co/murmur_hash.h"
#include "co/sha256.h"
#include "co/time.h"

namespace test {

DEF_test(hash) {
    DEF_case(base64) {
        EXPECT_EQ(co::base64_encode(""), "");
        EXPECT_EQ(co::base64_decode(""), "");
        EXPECT_EQ(co::base64_encode("hello world"), "aGVsbG8gd29ybGQ=");
        EXPECT_EQ(co::base64_decode("aGVsbG8gd29ybGQ="), "hello world");
        EXPECT_EQ(co::base64_decode("aGVsbG8gd29ybGQ=\r\n"), "hello world");
        EXPECT_EQ(co::base64_decode("aGVsbG8gd29ybGQ\r\n="), "");
        EXPECT_EQ(co::base64_decode("aGVs\nbG8gd29ybGQ="), "");
        EXPECT_EQ(co::base64_decode("aGVs\r\nbG8gd29ybGQ="), "hello world");

        auto s = co::to_string(time::mono.us());
        EXPECT_EQ(base64_decode(base64_encode(s)), s);
    }

    DEF_case(md5sum) {
        EXPECT_EQ(co::md5sum(""), "d41d8cd98f00b204e9800998ecf8427e");
        EXPECT_EQ(co::md5sum("hello world"), "5eb63bbbe01eeed093cb22bb8f5acdc3");
    }

    DEF_case(sha256sum) {
        EXPECT_EQ(co::sha256sum(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        EXPECT_EQ(co::sha256sum("hello"), "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
    }

    DEF_case(murmur_hash) {
        EXPECT_NE(co::murmur_hash("hello", 5), 0);
    }
}

} // test
