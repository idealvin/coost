#pragma once

#include "def.h"
#include "fastring.h"

namespace now {

/**
 * monotonic timestamp in milliseconds
 */
int64 ms();

/**
 * monotonic timestamp in microseconds
 */
int64 us();

// "%Y-%m-%d %H:%M:%S" ==> 2018-08-08 08:08:08
fastring str(const char* fm="%Y-%m-%d %H:%M:%S");

} // now

namespace epoch {

/**
 * milliseconds since epoch
 */
int64 ms();

/**
 * 
 * microseconds since epoch
 */
int64 us();

} // epoch

namespace ___ {
namespace sleep {

void ms(uint32 n);

void sec(uint32 n);

} // sleep
} // ___

using namespace ___;

class Timer {
  public:
    Timer() {
        _start = now::us();
    }

    void restart() {
        _start = now::us();
    }

    int64 us() const {
        return now::us() - _start;
    }

    int64 ms() const {
        return this->us() / 1000;
    }

  private:
    int64 _start;
};
