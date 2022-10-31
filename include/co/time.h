#pragma once

#include "def.h"
#include "fastring.h"

namespace now {

// monotonic timestamp in nanoseconds
__coapi int64 ns();

// monotonic timestamp in microseconds
__coapi int64 us();

// monotonic timestamp in milliseconds
__coapi int64 ms();

// "%Y-%m-%d %H:%M:%S" ==> 2018-08-08 08:08:08
__coapi fastring str(const char* fm = "%Y-%m-%d %H:%M:%S");

} // now

namespace epoch {

// microseconds since epoch
__coapi int64 us();

// milliseconds since epoch
__coapi int64 ms();

} // epoch

namespace ___ {
namespace sleep {

__coapi void ms(uint32 n);

__coapi void sec(uint32 n);

} // sleep
} // ___

using namespace ___;

class __coapi Timer {
  public:
    Timer() {
        _start = now::ns();
    }

    void restart() {
        _start = now::ns();
    }

    int64 ns() const {
        return now::ns() - _start;
    }

    int64 us() const {
        return this->ns() / 1000;
    }

    int64 ms() const {
        return this->ns() / 1000000;
    }

  private:
    int64 _start;
};
