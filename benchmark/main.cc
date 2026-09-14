#include "co/benchmark.h"

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    co::run_benchmarks();
    return 0;
}
