#include "co/benchmark.h"
#include "co/time.h"
#include <time.h>
#ifndef _WIN32
#include <sys/time.h>
#endif

// speed testing
// mac: 
//   gettimeofday > mono.ms(), mono.us() > clock_gettime > time(0) > co::now.str()
// linux:
//   time(0) > gettimeofday, mono.ms(), mono.us(), clock_gettime > co::now.str()
BM_group(time) {
    int64 v;
    co::string s;
    BM_add(co::now.str()) {
        s = co::now.str("%Y");
    }
    BM_use(s);

   // on linux: time(0) is fast, on mac: time(0) is slow
    BM_add(time(0)) {
        v = ::time(0);
    }
    BM_use(v);

    BM_add(co::mono_time.us()) {
        v = co::mono_time.us();
    }
    BM_use(v);

    BM_add(co::mono_time.ms()) {
        v = co::mono_time.ms();
    }
    BM_use(v);

#ifndef _WIN32
    struct timeval tv;
    BM_add(gettimeofday) {
        gettimeofday(&tv, 0);
    }
    BM_use(tv);

    struct timespec ts;
    BM_add(clock_gettime) {
      clock_gettime(CLOCK_MONOTONIC, &ts);
    }
    BM_use(ts);
#endif
}
