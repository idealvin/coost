#include "base/def.h"
#include "base/time.h"
#include "base/log.h"

#define def_test(n) \
    int N = n; \
    int64 _us; \
    Timer _t

#define def_case(func) \
    do { \
        _t.restart(); \
        for (int i = 0; i < N; ++i) { \
            func; \
        } \
        _us = _t.us(); \
        CLOG << #func << ":\t" << (_us * 1.0 / N) << " us"; \
    } while (0)
