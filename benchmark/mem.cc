#include "co/benchmark.h"
#include "co/mem.h"
#include "co/defer.h"

BM_group(mem) {
    void* a = ::malloc(32);
    void* b = co::alloc(32);
    void* c = ::malloc(8192);
    void* d = co::alloc(8192);
    defer(
        ::free(a);
        ::free(c);
        co::free(b, 32);
        co::free(d, 8192);
    )

    void* p;

    BM_add(malloc+free) {
        p = ::malloc(32);
        ::free(p);
    }
    BM_use(p);

    BM_add(co::alloc+free) {
        p = co::alloc(32);
        co::free(p, 32);
    }

    BM_sub_group_begin;
    BM_add(::malloc(32)) {
        p = ::malloc(32);
    }
    BM_use(p);

    BM_add(co::alloc(32)) {
        p = co::alloc(32);
    }
    BM_use(p);

    BM_sub_group_begin;
    BM_add(::malloc(64)) {
        p = ::malloc(64);
    }
    BM_use(p);

    BM_add(co::alloc(64)) {
        p = co::alloc(64);
    }
    BM_use(p);

    BM_add(co::alloc(32, 64)) {
        p = co::alloc(32, 64);
    }
    BM_use(p);

    BM_sub_group_begin;
    BM_add(::calloc(64)) {
        p = ::calloc(1, 64);
    }
    BM_use(p);

    BM_add(co::zalloc(64)) {
        p = co::zalloc(64);
    }
    BM_use(p);

    BM_add(co::alloc(64)+memset) {
        p = co::alloc(64);
        ::memset(p, 0, 64);
    }

    BM_sub_group_begin;
    BM_add(::malloc(4k)) {
        p = ::malloc(4096);
    }
    BM_use(p);

    BM_add(co::alloc(4k)) {
        p = co::alloc(4096);
    }
    BM_use(p);

    BM_sub_group_begin;
    BM_add(::malloc(8k)) {
        p = ::malloc(8192);
    }
    BM_use(p);

    BM_add(co::alloc(8k)) {
        p = co::alloc(8192);
    }
    BM_use(p);
}
