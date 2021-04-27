#include "co/hash.h"
#include "co/log.h"

int main(int argc, char** argv) {
    fastring s("hello world");
    COUT << "s: " << s;
    COUT << "base64 encode: " << base64_encode(s);
    COUT << "base64 decode: " << base64_decode(base64_encode(s));
    COUT << "crc16: " << crc16(s);
    COUT << "md5sum: " << md5sum(s);
    COUT << "hash32: " << hash32(s);
    COUT << "hash64: " << hash64(s);

    COUT << "base64 encode: " << base64_encode("");
    COUT << "base64 decode: " << base64_decode("");

    COUT << "base64 encode: " << base64_encode("xxx");
    COUT << "base64 decode: " << base64_decode(base64_encode("xxx"));

    COUT << "base64 encode: " << base64_encode("xxxxxxx");
    COUT << "base64 decode: " << base64_decode(base64_encode("xxxxxxx"));
    return 0;
}
