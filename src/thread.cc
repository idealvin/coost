#include "./thread.h"
#include "co/thread.h"
#include "co/time.h"
#include "co/clist.h"
#include "co/mem.h"


#ifdef _WIN32
inline uint32 _thread_id() {
    return GetCurrentThreadId();
}

#elif defined(__linux__)
#include <unistd.h>      // for syscall
#include <sys/syscall.h> // for SYS_xxx definitions

#ifndef SYS_gettid
#define SYS_gettid __NR_gettid
#endif

inline uint32 _thread_id() {
    return syscall(SYS_gettid);
}

#elif defined(__APPLE__)
inline uint32 _thread_id() {
    uint64 x;
    pthread_threadid_np(0, &x);
    return (uint32)x;
}

#elif defined(__FreeBSD__)
#include <pthread_np.h>

inline uint32 _thread_id() {
    return pthread_getthreadid_np();
}

#else
#include "co/atomic.h"

static uint32 g_id = 17700;

inline uint32 _thread_id() {
    return co::atomic_inc(&g_id, mo_relaxed);
}

#endif

__thread _cv* g_cv;

namespace co {
namespace xx {

__thread uint32 g_tid;

uint32 _thread_id() { return ::_thread_id(); }

} // xx

struct __cacheline_aligned sync_event_impl {
    struct _waitx : co::clink {
        _cv* cv;
    };

    sync_event_impl(bool manual_reset, bool signaled)
        : _wq(), _state(!signaled ? 0 : 2), _manual_reset(manual_reset) {
    }

    ~sync_event_impl() = default;

    bool _is_signaled() {
        return _manual_reset
            ? atomic_load(&_state, mo_acquire) == 2
            : atomic_bool_cas(&_state, 2, 0, mo_acquire, mo_relaxed);
    }

    void reset() {
        atomic_store(&_state, 0, mo_relaxed);
    }

    void wait() {
        if (_is_signaled()) return;

        _mutex_guard g(_m);
        if (_is_signaled()) return;

        const auto cv = _get_cv();
        cv->pred = false;
        _waitx w;
        w.cv = cv;
        _wq.push_back((clink*)&w);

        do {
            cv->wait(_m.native_handle());
        } while (!cv->pred);
    }

    bool wait(uint32 ms) {
        if (_is_signaled()) return true;
        if (ms == 0) return false;

        _mutex_guard g(_m);
        if (_is_signaled()) return true;

        const auto cv = _get_cv();
        cv->pred = false;
        _waitx w;
        w.cv = cv;
        _wq.push_back((clink*)&w);

        uint32 t = 0;
        time::timer timer;

        while (true) {
            bool r = cv->wait(_m.native_handle(), ms - t);
            if (cv->pred) return true; // signaled
            if (r) {
                t = (uint32) timer.ms();
                if (t < ms) continue; // spurious wakeup, continue waiting
            }
            _wq.erase(&w);
            return false; // timedout
        }
    }

    void notify_one() {
        if (atomic_bool_cas(&_state, 0, 1, mo_relaxed, mo_relaxed)) {
            _mutex_guard g(_m);
            if (!_wq.empty()) {
                _waitx* w = (_waitx*) _wq.pop_front();
                w->cv->pred = true;
                w->cv->notify_one();
                atomic_store(
                    &_state,
                    _manual_reset ? 2 : 0,
                    _manual_reset ? mo_release : mo_relaxed
                );
                return;
            }

            // no waiter notified
            atomic_store(&_state, 2, mo_release);
        }
    }

    void notify_all() {
        if (atomic_bool_cas(&_state, 0, 1, mo_relaxed, mo_relaxed)) {
            _mutex_guard g(_m);
            if (!_wq.empty()) {
                _wq.for_each([](co::clink* c) {
                    _waitx* w = (_waitx*) c;
                    w->cv->pred = true;
                    w->cv->notify_one();
                });
                _wq.clear();

                atomic_store(
                    &_state,
                    _manual_reset ? 2 : 0,
                    _manual_reset ? mo_release : mo_relaxed
                );
                return;
            }

            // no waiter notified
            atomic_store(&_state, 2, mo_release);
        }
    }

    _mutex _m;
    co::clist _wq;
    uint8 _state; // 0: unsignaled, 1: notifying, 2: signaled
    const bool _manual_reset;
};

sync_event::sync_event(bool manual_reset, bool signaled) {
    _p = co::alloc(sizeof(sync_event_impl), co::cache_line_size);
    runtime_assert(_p);
    new (_p) sync_event_impl(manual_reset, signaled);
}

sync_event::~sync_event() {
    if (_p) {
        ((sync_event_impl*)_p)->~sync_event_impl();
        co::free(_p, sizeof(sync_event_impl));
        _p = 0;
    }
}

void sync_event::notify_one() {
    ((sync_event_impl*)_p)->notify_one();
}

void sync_event::notify_all() {
    ((sync_event_impl*)_p)->notify_all();
}

void sync_event::reset() {
    ((sync_event_impl*)_p)->reset();
}

void sync_event::wait() {
    ((sync_event_impl*)_p)->wait();
}

bool sync_event::wait(uint32 ms) {
    return ((sync_event_impl*)_p)->wait(ms);
}

} // co
