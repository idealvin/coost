#include "co/all.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    while (true) sleep::sec(100000);

    return 0;
}
