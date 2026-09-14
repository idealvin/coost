#include "co/benchmark.h"
#include "co/rand.h"
#include <random>

BM_group(rand) {
    int a = ::rand();
    int b = co::rand();
    BM_use(a);
    BM_use(b);

    BM_sub_group_begin;
    int x;
    BM_add(::rand) {
        x = ::rand();
    }
    BM_use(x);

    BM_add(co::rand) {
        x = co::rand();
    }
    BM_use(x);

    uint32 seed = co::rand();
    BM_add(co::rand(seed)) {
        x = co::rand(seed);
    }
    BM_use(x);

    std::mt19937 m(std::random_device{}());
    BM_add(std::mt19937) {
        x = m();
    }
    BM_use(x);

    uint64 u;
    BM_add(co::rand64) {
        u = co::rand64();
    }
    BM_use(u);

    uint64 seed64 = co::rand64();
    BM_add(co::rand64(seed)) {
        u = co::rand64(seed64);
    }
    BM_use(u);

    BM_sub_group_begin;
    BM_add(co::randstr) {
        (void)co::randstr();
    }

    BM_add(co::randstr(charsets)) {
        (void) co::randstr("0-9a-f", 15);
    }

    char buf[16];
    BM_add(co::randchars) {
        co::randchars(buf, sizeof(buf));
    }
    BM_use(buf);
}
