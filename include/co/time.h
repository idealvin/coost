#pragma once

#include "def.h"
#include "fastring.h"

namespace co {
namespace now {

#ifdef _WIN32
namespace xx {

struct __coapi Initializer {
    Initializer();
    ~Initializer() = default;
};

static Initializer g_initializer;

} // xx
#endif

// monotonic timestamp in nanoseconds
__coapi int64 ns();

// monotonic timestamp in microseconds
__coapi int64 us();

// monotonic timestamp in milliseconds
__coapi int64 ms();

// "%Y-%m-%d %H:%M:%S" ==> 2023-01-07 18:01:23
__coapi fastring str(const char* fm = "%Y-%m-%d %H:%M:%S");

} // now

namespace epoch {

// microseconds since epoch
__coapi int64 us();

// milliseconds since epoch
__coapi int64 ms();

} // epoch

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

} // co

namespace now = co::now;
namespace epoch = co::epoch;

namespace _xx {
namespace sleep {

__coapi void ms(uint32 n);

__coapi void sec(uint32 n);

} // sleep
} // _xx

using namespace _xx;
