#include "co/tasked.h"
#include "co/time.h"
#include "co/thread.h"

namespace co {

struct tasked_impl {
    struct task {
        task(closure&& c, int period, int count)
            : c(std::move(c)), period(period), count(count) {
        }

        closure c;
        int period; // in seconds
        int count;
    };

    tasked_impl()
        : _stop(0), _ev(), _mtx() {
        _tasks.reserve(32);
        _new_tasks.reserve(32);
        std::thread(&tasked_impl::loop, this).detach();
    }

    ~tasked_impl() {
        this->stop();
    }

    void run_in(closure&& c, int sec) {
        std::lock_guard<std::mutex> g(_mtx);
        _new_tasks.emplace_back(std::move(c), 0, sec);
        if (sec <= 0) _ev.notify_one();
    }

    void run_every(closure&& c, int sec) {
        std::lock_guard<std::mutex> g(_mtx);
        _new_tasks.emplace_back(std::move(c), sec, sec);
    }

    void run_at(closure&& c, int hour, int minute, int second, bool daily);

    void stop();

    void loop();

    int _stop;
    co::vector<task> _tasks;
    co::vector<task> _new_tasks;
    co::sync_event _ev;
    std::mutex _mtx;
};

// if @daily is false, run c() only once, otherwise run c() every day at hour:minute:second
void tasked_impl::run_at(closure&& c, int hour, int minute, int second, bool daily) {
    runtime_assert(0 <= hour && hour <= 23);
    runtime_assert(0 <= minute && minute <= 59);
    runtime_assert(0 <= second && second <= 59);

    co::string t = co::now.str("%H%M%S");
    int now_hour = (t[0] - '0') * 10 + (t[1] - '0');
    int now_min  = (t[2] - '0') * 10 + (t[3] - '0');
    int now_sec  = (t[4] - '0') * 10 + (t[5] - '0');

    int now_seconds = now_hour * 3600 + now_min * 60 + now_sec;
    int seconds = hour * 3600 + minute * 60 + second;
    if (seconds < now_seconds) seconds += 86400;
    int diff = seconds - now_seconds;

    std::lock_guard<std::mutex> g(_mtx);
    _new_tasks.emplace_back(std::move(c), (daily ? 86400 : 0), diff);
}

void tasked_impl::loop() {
    int64 ms = 0;
    int sec = 0;
    co::timer timer;
    co::vector<task> tmp;

    while (!_stop) {
        timer.restart();
        {
            std::lock_guard<std::mutex> g(_mtx);
            if (!_new_tasks.empty()) _new_tasks.swap(tmp);
        }

        if (!tmp.empty()) {
            for (auto& x : tmp) _tasks.emplace_back(std::move(x));
            tmp.clear();
            if (tmp.capacity() >= 4096) co::vector<task>().swap(tmp);
        }

        if (ms >= 1000) {
            sec = (int) (ms / 1000);
            ms -= sec * 1000;
        }

        for (size_t i = 0; i < _tasks.size();) {
            auto& t = _tasks[i];
            if ((t.count -= sec) <= 0) {
                t.c();
                if (t.period > 0) {
                    t.count = t.period;
                    ++i;
                } else {
                    if (i < _tasks.size() - 1) t = std::move(_tasks.back());
                    _tasks.pop_back();
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

void tasked_impl::stop() {
    int x = atomic_cas(&_stop, 0, 1);
    if (x == 0) {
        _ev.notify_one();
        while (_stop != 2) time::sleep(1);

        std::lock_guard<std::mutex> g(_mtx);
        _tasks.clear();
        _new_tasks.clear();
    } else {
        while (_stop != 2) time::sleep(1);
    }
}

tasked::tasked() {
    _p = co::alloc(sizeof(tasked_impl), co::cache_line_size);
    runtime_assert(_p);
    new (_p) tasked_impl();
}

tasked::~tasked() {
    if (_p) {
        ((tasked_impl*)_p)->~tasked_impl();
        co::free(_p, sizeof(tasked_impl));
        _p = 0;
    }
}

void tasked::run_in(closure&& c, int sec) {
    ((tasked_impl*)_p)->run_in(std::move(c), sec);
}

void tasked::run_every(closure&& c, int sec) {
    ((tasked_impl*)_p)->run_every(std::move(c), sec);
}

void tasked::run_at(closure&& c, int hour, int minute, int second) {
    ((tasked_impl*)_p)->run_at(std::move(c), hour, minute, second, false);
}

void tasked::run_daily(closure&& c, int hour, int minute, int second) {
    ((tasked_impl*)_p)->run_at(std::move(c), hour, minute, second, true);
}

void tasked::stop() {
    ((tasked_impl*)_p)->stop();
}

} // co
