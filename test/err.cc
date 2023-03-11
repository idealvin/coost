#include "co/co.h"
#include "co/cout.h"

int main(int argc, char** argv) {
    for (int i = -3; i < 140; ++i) {
        co::print("error: ", i, "  str: ", co::strerror(i));
    }

    for (int i = 10060; i < 10063; ++i) {
        co::print("error: ", i, "  str: ", co::strerror(i));
    }

    return 0;
}
