#include "base/hash.h"
#include "base/log.h"

int main(int argc, char** argv) {
    fastring s("hello world");
    COUT << "s: " << s;
    COUT << "base64 encode: " << base64_encode(s);
    COUT << "base64 decode: " << base64_decode(base64_encode(s));
    COUT << "crc16: " << crc16(s);
    COUT << "md5sum: " << md5sum(s);
    COUT << "hash32: " << hash32(s);
    COUT << "hash64: " << hash64(s);

    return 0;
}
