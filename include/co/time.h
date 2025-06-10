#pragma once

#include "def.h"
#include "fastring.h"

namespace _xx {
namespace time {
namespace xx {

#ifdef _WIN32
struct TimeInit {
    TimeInit();
    ~TimeInit() = default;
};

static TimeInit g_time_init;
#endif

// unix time
struct Unix {
    // nanoseconds since epoch, may overflow at the year 2262
    static int64 ns();

    // microseconds since epoch
    static int64 us();

    // milliseconds since epoch
    static int64 ms();
};

// monotonic timestamp
struct Mono {
    static int64 ns();
    static int64 us();
    static int64 ms();
};

} // xx

#undef unix

extern xx::Mono mono;
extern xx::Unix unix;

struct timer {
    timer() {
        _start = mono.ns();
    }

    void restart() {
        _start = mono.ns();
    }

    int64 ns() const {
        return mono.ns() - _start;
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

// sleep for @ms milliseconds
void sleep(uint32 ms);

fastring str(const char* fmt="%Y-%m-%d %H:%M:%S");

} // time
} // _xx

using namespace _xx;
