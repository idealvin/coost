#include "co/all.h"

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    co::print("hello coost");
    return 0;
}
