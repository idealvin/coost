#include "co/all.h"
#include "co/benchmark.h"

BM_group(atomic) {
    int i = 0;

    BM_run(++)(
        atomic_inc(&i);
    );

    BM_run(--)(
        atomic_dec(&i);
    );
}

BM_group(malloc) {
    void* p;

    BM_run(malloc)(
        p = ::malloc(32);
    );
    BM_use(p);

    BM_run(co::alloc)(
        p = co::alloc(32);
    );
}

BM_group(malloc_free) {
    void* p;

    BM_run(malloc+free)(
        p = ::malloc(32);
        ::free(p);
    );
    BM_use(p);

    BM_run(co::alloc+free)(
        p = co::alloc(32);
        co::free(p, 32);
    );
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    bm::run_benchmarks();
    return 0;
}
