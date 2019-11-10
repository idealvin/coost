#pragma once

#include "def.h"
#include "fastring.h"

namespace now {

int64 ms(); // now in milliseconds
int64 us(); // now in microseconds

// "%Y-%m-%d %H:%M:%S" ==> 2018-08-08 08:08:08
fastring str(const char* fm="%Y-%m-%d %H:%M:%S");

} // now

namespace ___ {
namespace sleep {

void ms(unsigned int n);

void sec(unsigned int n);

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
