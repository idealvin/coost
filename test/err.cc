#include "co/error.h"
#include "co/print.h"

int main(int argc, char** argv) {
    for (int i = -3; i < 140; ++i) {
        co::print("error: ", i, "  str: ", co::strerror(i), '\n');
    }

    for (int i = 10060; i < 10063; ++i) {
        co::print("error: ", i, "  str: ", co::strerror(i), '\n');
    }

    co::print().flush();
    return 0;
}
