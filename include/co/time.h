#pragma once

#include "string.h"

namespace co {
namespace xx {

// time since epoch (the unix time)
struct Now {
    // nanoseconds since epoch, may overflow at the year 2262
    static int64 ns();

    // microseconds since epoch
    static int64 us();

    // milliseconds since epoch
    static int64 ms();

    // formatted time string
    static co::string str(const char* fmt="%Y-%m-%d %H:%M:%S");
};

// monotonic timestamp
struct MonoTime {
    static int64 ns();
    static int64 us() { return ns() / 1000; }
    static int64 ms() { return ns() / 1000000; }
};

} // xx

inline constexpr xx::Now now{};
inline constexpr xx::MonoTime mono_time{};

struct timer {
    timer() {
        _start = mono_time.ns();
    }

    void restart() {
        _start = mono_time.ns();
    }

    int64 ns() const {
        return mono_time.ns() - _start;
    }

    int64 us() const {
        return this->ns() / 1000;
    }

    int64 ms() const {
        return this->ns() / 1000000;
    }

    int64 _start;
};

} // co

namespace __co {
namespace time {

// sleep for @ms milliseconds
void sleep(uint32 ms);

} // time
} // __co

using namespace __co;
