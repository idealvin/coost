#include "co/tasked.h"
#include "co/vector.h"
#include "co/time.h"
#include "co/co/thread.h"

namespace co {
namespace xx {

class TaskedImpl {
  public:
    typedef std::function<void()> F;

    struct Task {
        Task(F&& f, int p, int c)
            : fun(std::move(f)), period(p), count(c) {
        }
        F fun;
        int period; // in seconds
        int count;
    };

    TaskedImpl()
        : _stop(0), _tasks(32), _new_tasks(32), _ev(), _mtx() {
        std::thread(&TaskedImpl::loop, this).detach();
    }

    ~TaskedImpl() {
        this->stop();
    }

    void run_in(F&& f, int sec) {
        std::lock_guard<std::mutex> g(_mtx);
        _new_tasks.emplace_back(std::move(f), 0, sec);
        if (sec <= 0) _ev.signal();
    }

    void run_every(F&& f, int sec) {
        std::lock_guard<std::mutex> g(_mtx);
        _new_tasks.emplace_back(std::move(f), sec, sec);
    }

    void run_at(F&& f, int hour, int minute, int second, bool daily);

    void stop();

  private:
    void loop();

  private:
    int _stop;
    co::vector<Task> _tasks;
    co::vector<Task> _new_tasks;
    co::sync_event _ev;
    std::mutex _mtx;
};

// if @daily is false, run f() only once, otherwise run f() every day at hour:minute:second
void TaskedImpl::run_at(F&& f, int hour, int minute, int second, bool daily) {
    assert(0 <= hour && hour <= 23);
    assert(0 <= minute && minute <= 59);
    assert(0 <= second && second <= 59);

    fastring t = now::str("%H%M%S");
    int now_hour = (t[0] - '0') * 10 + (t[1] - '0');
    int now_min  = (t[2] - '0') * 10 + (t[3] - '0');
    int now_sec  = (t[4] - '0') * 10 + (t[5] - '0');

    int now_seconds = now_hour * 3600 + now_min * 60 + now_sec;
    int seconds = hour * 3600 + minute * 60 + second;
    if (seconds < now_seconds) seconds += 86400;
    int diff = seconds - now_seconds;

    std::lock_guard<std::mutex> g(_mtx);
    _new_tasks.emplace_back(std::move(f), (daily ? 86400 : 0), diff);
}

void TaskedImpl::loop() {
    int64 ms = 0;
    int sec = 0;
    co::Timer timer;
    co::vector<Task> tmp(32);

    while (!_stop) {
        timer.restart();
        {
            std::lock_guard<std::mutex> g(_mtx);
            if (!_new_tasks.empty()) _new_tasks.swap(tmp);
        }

        if (!tmp.empty()) {
            _tasks.append(std::move(tmp));
            tmp.clear();
        }

        if (ms >= 1000) {
            sec = (int) (ms / 1000);
            ms -= sec * 1000;
        }

        for (size_t i = 0; i < _tasks.size();) {
            auto& t = _tasks[i];
            if ((t.count -= sec) <= 0) {
                t.fun();
                if (t.period > 0) {
                    t.count = t.period;
                    ++i;
                } else {
                    _tasks.remove(i);
                }
            } else {
                ++i;
            }
        }

        _ev.wait(1000);
        if (_stop) { atomic_store(&_stop, 2); return; }
        ms += timer.ms();
    }
}

void TaskedImpl::stop() {
    int x = atomic_cas(&_stop, 0, 1);
    if (x == 0) {
        _ev.signal();
        while (_stop != 2) sleep::ms(1);
        std::lock_guard<std::mutex> g(_mtx);
        _tasks.clear();
        _new_tasks.clear();
    } else if (x == 1) {
        while (_stop != 2) sleep::ms(1);
    }
}

} // xx

Tasked::Tasked() {
    _p = co::make<xx::TaskedImpl>();
}

Tasked::~Tasked() {
    if (_p) {
        co::del((xx::TaskedImpl*)_p);
        _p = 0;
    }
}

void Tasked::run_in(F&& f, int sec) {
    ((xx::TaskedImpl*)_p)->run_in(std::move(f), sec);
}

void Tasked::run_every(F&& f, int sec) {
    ((xx::TaskedImpl*)_p)->run_every(std::move(f), sec);
}

void Tasked::run_at(F&& f, int hour, int minute, int second) {
    ((xx::TaskedImpl*)_p)->run_at(std::move(f), hour, minute, second, false);
}

void Tasked::run_daily(F&& f, int hour, int minute, int second) {
    ((xx::TaskedImpl*)_p)->run_at(std::move(f), hour, minute, second, true);
}

void Tasked::stop() {
    ((xx::TaskedImpl*)_p)->stop();
}

} // co
