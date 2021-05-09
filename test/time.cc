#include "test.h"
#include "co/time.h"
#include "co/str.h"
#include "co/fastream.h"
#include <time.h>
#ifndef _WIN32
#include <sys/time.h>
#endif

// speed testing
// mac: 
//   gettimeofday > now::ms(), now::us() > clock_gettime > time(0) > now::str()
// linux:
//   time(0) > gettimeofday, now::ms(), now::us(), clock_gettime > now::str()
int main(int argc, char** argv) {
    def_test(10000);

    fastring s;
    def_case(now::str("%Y"));
    def_case(now::str());

    int64 v; (void)v;
    def_case(v = now::ms());
    def_case(v = now::us());
    
    // on linux: time(0) is fast, on mac: time(0) is slow
    def_case(v = time(0));

  #ifndef _WIN32
    struct timeval tv;
    struct timespec ts;
    def_case(gettimeofday(&tv, 0));
    def_case(clock_gettime(CLOCK_MONOTONIC, &ts));
  #endif

    return 0;
}
