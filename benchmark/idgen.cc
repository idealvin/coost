#include "co/benchmark.h"
#include "../src/co/idgen.h"

BM_group(idgen) {
    int i;
    co::IdGen g;
    i = g.pop();

    BM_add(pop) {
        i = g.pop();
    }
    BM_use(i);

    BM_add(pop1+push1) {
        i = g.pop();
        g.push(i);
    }
    BM_use(i);

    int x[8];
    BM_add(pop8+push8) {
        x[0] = g.pop();
        x[1] = g.pop();
        x[2] = g.pop();
        x[3] = g.pop();
        x[4] = g.pop();
        x[5] = g.pop();
        x[6] = g.pop();
        x[7] = g.pop();
        g.push(x[0]);
        g.push(x[1]);
        g.push(x[2]);
        g.push(x[3]);
        g.push(x[4]);
        g.push(x[5]);
        g.push(x[6]);
        g.push(x[7]);
    }
    BM_use(x);
}
