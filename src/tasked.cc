#include "co/tasked.h"
#include "co/array.h"
#include "co/time.h"
#include "co/thread.h"

class TaskedImpl {
  public:
    typedef std::function<void()> F;

    struct Task {
        Task(F&& f, int p, int c)
            : fun(f), period(p), count(c) {
        }

        F fun;
        int period; // in seconds
        int count;
    };

    TaskedImpl()
        : _stop(0), _mtx(), _ev(), _t(&TaskedImpl::loop, this) {
    }

    ~TaskedImpl() {
        this->stop();
    }

    void run_in(F&& f, int sec) {
        MutexGuard g(_mtx);
        _tmp.push_back(co::make<Task>(std::move(f), 0, sec));
        if (sec <= 0) _ev.signal();
    }

    void run_every(F&& f, int sec) {
        MutexGuard g(_mtx);
        _tmp.push_back(co::make<Task>(std::move(f), sec, sec));
    }

    void run_at(F&& f, int hour, int minute, int second, bool daily);

    void stop();

  private:
    void loop();

    void clear(co::array<Task*>& v) {
        for (size_t i = 0; i < v.size(); ++i) co::del(v[i]);
        v.clear();
    }

  private:
    bool _stop;
    Mutex _mtx;
    SyncEvent _ev;
    Thread _t;

    co::array<Task*> _tasks;
    co::array<Task*> _tmp;
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

    MutexGuard g(_mtx);
    _tmp.push_back(co::make<Task>(std::move(f), (daily ? 86400 : 0), diff));
}

void TaskedImpl::loop() {
    int64 ms = 0;
    int sec = 0;
    Timer t;
    co::array<Task*> tmp;

    while (!_stop) {
        t.restart();
        {
            MutexGuard g(_mtx);
            if (!_tmp.empty()) _tmp.swap(tmp);
        }

        if (!tmp.empty()) {
            _tasks.push_back(tmp.data(), tmp.size());
            tmp.clear();
        }

        if (ms >= 1000) {
            sec = (int) (ms / 1000);
            ms -= sec * 1000;
        }

        for (size_t i = 0; i < _tasks.size();) {
            Task* task = _tasks[i];
            if ((task->count -= sec) <= 0) {
                task->fun();
                if (task->period > 0) {
                    task->count = task->period;
                    ++i;
                } else {
                    co::del(task);
                    if (i != _tasks.size() - 1) _tasks[i] = _tasks.back();
                    // remove the tail task, don't ++i here.
                    _tasks.pop_back();
                }
            } else {
                ++i;
            }
        }

        _ev.wait(1000);
        if (_stop) return;
        ms += t.ms();
    }
}

void TaskedImpl::stop() {
    if (atomic_swap(&_stop, 1) == 0) {
        _ev.signal();
        _t.join();
        MutexGuard g(_mtx);
        this->clear(_tasks);
        this->clear(_tmp);
    }
}

Tasked::Tasked() {
    _p = co::make<TaskedImpl>();
}

Tasked::~Tasked() {
    if (_p) {
        co::del((TaskedImpl*)_p);
        _p = 0;
    }
}

void Tasked::run_in(F&& f, int sec) {
    ((TaskedImpl*)_p)->run_in(std::move(f), sec);
}

void Tasked::run_every(F&& f, int sec) {
    ((TaskedImpl*)_p)->run_every(std::move(f), sec);
}

void Tasked::run_at(F&& f, int hour, int minute, int second) {
    ((TaskedImpl*)_p)->run_at(std::move(f), hour, minute, second, false);
}

void Tasked::run_daily(F&& f, int hour, int minute, int second) {
    ((TaskedImpl*)_p)->run_at(std::move(f), hour, minute, second, true);
}

void Tasked::stop() {
    ((TaskedImpl*)_p)->stop();
}
