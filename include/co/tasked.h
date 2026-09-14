#pragma once

#include "closure.h"

namespace co {

// timed task scheduler
struct tasked {
    tasked();
    ~tasked();

    tasked(tasked&& t) : _p(t._p) {
        t._p = 0;
    }

    tasked(const tasked&) = delete;
    void operator=(const tasked&) = delete;
    void operator=(tasked&&) = delete;

    // run c() once @sec seconds later
    void run_in(closure&& c, int sec);

    // run c() every @sec seconds
    void run_every(closure&& c, int sec);

    // run c() once at hour:minute:second
    // hour: 0-23, mimute & second: 0-59
    void run_at(closure&& c, int hour, int minute=0, int second=0);

    // run c() at hour:minute:second every day
    // hour: 0-23, mimute & second: 0-59
    void run_daily(closure&& c, int hour=0, int minute=0, int second=0);

    // stop this task scheduler
    void stop();

    void* _p;
};

} // co
