#pragma once

#include "def.h"
#include "array.h"
#include "time.h"

namespace bm {

__coapi void run_benchmarks();

namespace xx {

struct Result {
    Result(const char* bm, double ns) noexcept : bm(bm), ns(ns) {}
    const char* bm;
    double ns;
};

struct Group {
    Group(const char* name, void (*f)(Group&)) noexcept : name(name), f(f) {}
    const char* name;
    const char* bm;
    void (*f)(Group&);
    int iters;
    int64 ns;
    Timer timer;
    co::array<Result> res;
};

__coapi bool add_group(const char* name, void (*f)(Group&));
__coapi int calc_iters(int64 ns);
__coapi void use(void* p, int n);

} // xx

// define a benchmark group
#define BM_group(_name_) \
    void _co_bm_group_##_name_(bm::xx::Group&); \
    static bool _co_bm_v_##_name_ = bm::xx::add_group(#_name_, _co_bm_group_##_name_); \
    void _co_bm_group_##_name_(bm::xx::Group& _g_)

// add a benchmark, it must be inside BM_group
#define BM_add(_name_) _g_.bm = #_name_; _BM_add

#define _BM_add(e) { \
    auto _f_ = [&]() {e;}; \
    _g_.timer.restart(); \
    _f_(); \
    _g_.ns = _g_.timer.ns(); \
    _g_.iters = bm::xx::calc_iters(_g_.ns); \
    if (_g_.iters > 1) { \
        _g_.timer.restart(); \
        for (int _i_ = 0; _i_ < _g_.iters; ++_i_) { _f_(); } \
        _g_.ns = _g_.timer.ns(); \
    } \
    _g_.res.push_back(bm::xx::Result(_g_.bm, _g_.ns * 1.0 / _g_.iters)); \
}

// tell the compiler do not optimize this away
#define BM_use(v) bm::xx::use(&v, sizeof(v))

} // bm
