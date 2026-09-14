#include "co/benchmark.h"
#include "co/atomic.h"

BM_group(atomic) {
    __cacheline_aligned int i = 0;

    BM_add(atomic_inc) {
        co::atomic_inc(&i);
    }
    BM_use(i);

    BM_add(atomic_dec) {
        co::atomic_dec(&i);
    }
    BM_use(i);

    BM_add(atomic_cas) {
        co::atomic_cas(&i, 0, 1);
    }
    BM_use(i);

    BM_add(atomic_or) {
        co::atomic_or(&i, 11);
    }
    BM_use(i);
}
