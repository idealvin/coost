#include "co/all.h"

int main(int argc, char** argv) {
    FLG_help = "nice!";
    flag::init(argc, argv);
    log::init();

    COUT << "hello world";

    return 0;
}
