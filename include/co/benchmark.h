#pragma once

#include "def.h"
#include "flag.h"
#include "time.h"

namespace co {

void run_benchmarks();

namespace bm {

struct Result {
    Result(const char* bm, double ns, bool as_bench) noexcept
        : bm(bm), ns(ns), as_bench(as_bench) {
    }
    const char* bm;
    double ns;
    bool as_bench;
};

struct Group {
    Group(const char* name, void (*f)(Group&), bool& e) noexcept
        : name(name), f(f), enabled(e) {
    }

    void start(const char* bm) noexcept;

    void end() noexcept {
        const int64 t = timer.ns();
        if (iters > 1) res.back().ns = t * 1.0 / iters;
    }

    void _the_first_call() noexcept {
        const uint64 ns = timer.ns();

        // warmup
        if (res.back().ns == 0 && ns <= 300 * 1000 * 1000) {
            res.back().ns = ns * 1.0;
            timer.restart();
            return;
        }

        res.back().ns = ns * 1.0;
        c = iters = [](int64 ns) {
            if (ns <= 1000) return 100 * 1000; // 0-1us
            if (ns <= 10000) return 10 * 1000; // 1-10us
            if (ns <= 100000) return 1000;     // 10-100us
            if (ns <= 1000000) return 100;     // 100-1000us
            if (ns <= 10000000) return 10;     // 1-10ms
            if (ns <= 100000000) return 5;     // 10-100ms
            return 1;
        }(ns);
        iters > 1 ? timer.restart() : (void)(c = 0);
    }

    bool done() const noexcept { return c == 0; }
    void goon() noexcept { iters != 0 ? (void)--c : _the_first_call(); }

    const char* name;
    void (*f)(Group&);
    bool& enabled;
    int c;
    int iters;
    time::timer timer;
    co::vector<Result> res;
};

struct Runner {
    Runner(Group& g, const char* bm) : g(g) { g.start(bm); }
    ~Runner() { g.end(); }
    Group& g;
};

bool add_group(const char* name, void (*f)(Group&), bool& e);
void use(void* p, int n);
void sub_group_begin();

} // bm
} // co

// define a benchmark group
#define BM_group(name) \
    DEF_bool(name, false, "enable this benchmark test if true"); \
    void _co_bm_##name(co::bm::Group&); \
    static bool _co_bm_v_##name = co::bm::add_group(#name, _co_bm_##name, FLG_##name); \
    void _co_bm_##name(co::bm::Group& _co_bmg_)

// add a test to the current group
#define BM_add(name) \
    for (co::bm::Runner _co_bmr_(_co_bmg_, #name); !_co_bmg_.done(); _co_bmg_.goon())

#define BM_sub_group_begin co::bm::sub_group_begin();

// tell the compiler do not optimize this away
#define BM_use(v) co::bm::use(&v, sizeof(v));
