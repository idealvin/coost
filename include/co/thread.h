#pragma once

#ifdef _WIN32
#include "win/thread.h"
#else
#include "unix/thread.h"
#endif

class TaskSched {
  public:
    typedef std::function<void()> F;

    TaskSched();
    ~TaskSched();

    TaskSched(TaskSched&& t) {
        _p = t._p;
        t._p = 0;
    }

    // run f() immediately
    void run(F&& f);

    void run(const F& f) {
        this->run(F(f));
    }

    // run f() once @sec seconds later
    void run_in(F&& f, int sec);

    void run_in(const F& f, int sec) {
        this->run_in(F(f), sec);
    }

    // run f() every @sec seconds
    void run_every(F&& f, int sec);

    void run_every(const F& f, int sec) {
        this->run_every(F(f), sec);
    }

    // run_daily(f, 23, 0, 0);  ->  run f() at 23:00:00 every day
    void run_daily(F&& f, int hour=0, int min=0, int sec=0);

    void run_daily(const F& f, int hour=0, int min=0, int sec=0) {
        this->run_daily(F(f), hour, min, sec);
    }

    // stop the task schedule
    void stop();

  private:
    void* _p;
    DISALLOW_COPY_AND_ASSIGN(TaskSched);
};
