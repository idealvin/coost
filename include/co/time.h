#pragma once

#include "def.h"
#include "fastring.h"

namespace now {

// monotonic timestamp in milliseconds
__coapi int64 ms();

// monotonic timestamp in microseconds
__coapi int64 us();

// "%Y-%m-%d %H:%M:%S" ==> 2018-08-08 08:08:08
__coapi fastring str(const char* fm = "%Y-%m-%d %H:%M:%S");

} // now

namespace epoch {

// milliseconds since epoch
__coapi int64 ms();

// microseconds since epoch
__coapi int64 us();

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
