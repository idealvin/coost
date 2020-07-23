#include "co/thread.h"
#include "co/time.h"
#include <vector>

#ifndef _WIN32
#ifdef __linux__
#include <unistd.h>       // for syscall()
#include <sys/syscall.h>  // for SYS_xxx definitions
#include <time.h>         // for clock_gettime
#else
#include <sys/time.h>     // for gettimeofday
#endif

namespace xx {

#ifdef __linux__
#ifndef SYS_gettid
#define SYS_gettid __NR_gettid
#endif

unsigned int gettid() {
    return syscall(SYS_gettid);
}

#else /* for mac, bsd */
unsigned int gettid() {
    uint64 x;
    pthread_threadid_np(0, &x);
    return (unsigned int) x;
}
#endif

} // xx

SyncEvent::SyncEvent(bool manual_reset, bool signaled)
    : _consumer(0), _manual_reset(manual_reset), _signaled(signaled) {
  #ifdef __linux__
    pthread_condattr_t attr;
    int r = pthread_condattr_init(&attr);
    assert(r == 0);

    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    r = pthread_cond_init(&_cond, &attr);
    assert(r == 0);

    pthread_condattr_destroy(&attr);
  #else
    int r = pthread_cond_init(&_cond, NULL);
    assert(r == 0);
  #endif
}

void SyncEvent::wait() {
    MutexGuard g(_mutex);
    if (!_signaled) {
        ++_consumer;
        pthread_cond_wait(&_cond, _mutex.mutex());
        --_consumer;
        assert(_signaled);
    }

    if (!_manual_reset && _consumer == 0) _signaled = false;
}

bool SyncEvent::wait(unsigned int ms) {
    MutexGuard g(_mutex);
    if (!_signaled) {
      #ifdef __linux__
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
      #else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct timespec ts;
        ts.tv_sec = tv.tv_sec;
        ts.tv_nsec = tv.tv_usec * 1000;
      #endif

        if (ms < 1000) {
            ts.tv_nsec += ms * 1000000;
        } else {
            unsigned int x = ms / 1000;
            ts.tv_nsec += (ms - x * 1000) * 1000000;
            ts.tv_sec += x;
        }

        if (ts.tv_nsec > 999999999) {
            ts.tv_nsec -= 1000000000;
            ++ts.tv_sec;
        }

        ++_consumer;
        int r = pthread_cond_timedwait(&_cond, _mutex.mutex(), &ts);
        --_consumer;

        if (r == ETIMEDOUT) return false;
        assert(r == 0);
        assert(_signaled);
    }

    if (!_manual_reset && _consumer == 0) _signaled = false;
    return true;
}
#endif

class TaskSchedImpl {
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

    TaskSchedImpl()
        : _stop(0), _mtx(), _ev(), _t(&TaskSchedImpl::_Run, this) {
    }

    ~TaskSchedImpl() {
        this->stop();
    }

    void run(F&& f) {
        {
            MutexGuard g(_mtx);
            _tmp.push_back(new Task(std::move(f), 0, 0));
        }
        _ev.signal();
    }

    void run_in(F&& f, int sec) {
        MutexGuard g(_mtx);
        _tmp.push_back(new Task(std::move(f), 0, sec));
    }

    void run_every(F&& f, int sec) {
        MutexGuard g(_mtx);
        _tmp.push_back(new Task(std::move(f), sec, sec));
    }

    void run_daily(F&& f, int hour, int min, int sec);

    void stop();

  private:
    bool _stop;
    Mutex _mtx;
    SyncEvent _ev;
    Thread _t;

    std::vector<Task*> _tasks;
    std::vector<Task*> _tmp;

    void _Run();

    void _Clear(std::vector<Task*>& v) {
        for (size_t i = 0; i < v.size(); ++i) delete v[i];
        v.clear();
    }
};

void TaskSchedImpl::run_daily(F&& f, int hour, int min, int sec) {
    fastring t = now::str("%H%M%S");
    int now_hour = (t[0] - '0') * 10 + (t[1] - '0');
    int now_min = (t[2] - '0') * 10 + (t[3] - '0');
    int now_sec = (t[4] - '0') * 10 + (t[5] - '0');

    int now_seconds = now_hour * 3600 + now_min * 60 + now_sec;
    int seconds = hour * 3600 + min * 60 + sec;
    if (seconds < now_seconds) seconds += 86400;
    int diff = seconds - now_seconds;

    MutexGuard g(_mtx);
    _tmp.push_back(new Task(std::move(f), 86400, diff));
}

void TaskSchedImpl::_Run() {
    int64 ms = 0;
    Timer t;

    while (!_stop) {
        _ev.wait(1000);
        if (_stop) return;

        t.restart();
        {
            MutexGuard g(_mtx);
            if (!_tmp.empty()) {
                _tasks.insert(_tasks.end(), _tmp.begin(), _tmp.end());
                _tmp.clear();
            }
        }

        int sec = 0;
        if (ms >= 1000) {
            sec = (int) (ms / 1000);
            ms -= sec * 1000;
        }

        for (size_t i = 0; i < _tasks.size();) {
            Task* task = _tasks[i];
            if ((task->count -= (sec + 1)) <= 0) {
                task->fun();
                if (task->period > 0) {
                    task->count = task->period;
                    ++i;
                } else {
                    delete task;
                    if (i != _tasks.size() - 1) _tasks[i] = _tasks.back();
                    _tasks.pop_back();
                }
            } else {
                ++i;
            }
        }

        ms += t.ms();
    }
}

void TaskSchedImpl::stop() {
    if (atomic_swap(&_stop, 1) == 0) {
        _ev.signal();
        _t.join();

        MutexGuard g(_mtx);
        this->_Clear(_tasks);
        this->_Clear(_tmp);
    }
}

TaskSched::TaskSched() {
    _p = new TaskSchedImpl();
}

TaskSched::~TaskSched() {
    if (_p) {
        delete (TaskSchedImpl*) _p;
        _p = 0;
    }
}

void TaskSched::run(F&& f) {
    ((TaskSchedImpl*)_p)->run(std::move(f));
}

void TaskSched::run_in(F&& f, int sec) {
    ((TaskSchedImpl*)_p)->run_in(std::move(f), sec);
}

void TaskSched::run_every(F&& f, int sec) {
    ((TaskSchedImpl*)_p)->run_every(std::move(f), sec);
}

void TaskSched::run_daily(F&& f, int hour, int min, int sec) {
    ((TaskSchedImpl*)_p)->run_daily(std::move(f), hour, min, sec);
}

void TaskSched::stop() {
    ((TaskSchedImpl*)_p)->stop();
}
