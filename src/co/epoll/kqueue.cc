#if !defined(_WIN32) && !defined(__linux__)
#include "kqueue.h"
#include "co/error.h"
#include "co/log.h"
#include "../sched.h"
#include "../../close.h"

#include <time.h>
#include <unistd.h>
#include <sys/event.h>

namespace co {

using Ev = struct kevent;

Kqueue::Kqueue() : _signaled(0), _n(0) {
    _kq = kqueue();
    runtime_assert(_kq != -1);

    Ev ev;
    EV_SET(&ev, 0, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
    runtime_assert(::kevent(_kq, &ev, 1, nullptr, 0, nullptr) != -1);

    _events = (Ev*) co::_static_alloc(N * sizeof(Ev), co::cache_line_size);
    ::memset(_events, 0, N * sizeof(Ev));
}

Kqueue::~Kqueue() {
    _close(_kq);
}

// EV_CLEAR for edge-triggered
bool Kqueue::add_ev_read(int fd, void* c) {
    Ev ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, c);
    const int r = ::kevent(_kq, &ev, 1, nullptr, 0, nullptr);
    if (r >= 0) return true;

    const int e = errno;
    log::error("kqueue add ev_read error: ", co::strerror(e), ", fd: ", fd, " co: ", c);
    return false;
}

bool Kqueue::add_ev_write(int fd, void* c) {
    Ev ev;
    EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, c);
    const int r = ::kevent(_kq, &ev, 1, nullptr, 0, nullptr);
    if (r >= 0) return true;

    const int e = errno;
    log::error("kqueue add ev_write error: ", co::strerror(e), ", fd: ", fd, " co: ", c);
    return false;
}

void Kqueue::del_ev_read(int fd) {
    Ev ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    if (::kevent(_kq, &ev, 1, nullptr, 0, nullptr) < 0 && errno != ENOENT) {
        const int e = errno;
        log::error("kqueue del ev_read error: ", co::strerror(e), ", fd: ", fd);
    }
}

void Kqueue::del_ev_write(int fd) {
    Ev ev;
    EV_SET(&ev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    if (::kevent(_kq, &ev, 1, nullptr, 0, nullptr) < 0 && errno != ENOENT) {
        const int e = errno;
        log::error("kqueue del ev_write error: ", co::strerror(e), ", fd: ", fd);
    }
}

void Kqueue::del_event(int fd) {
    Ev ev[2];
    EV_SET(&ev[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&ev[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    if (::kevent(_kq, ev, 2, nullptr, 0, nullptr) < 0 && errno != ENOENT) {
        const int e = errno;
        log::error("kqueue del event error: ", co::strerror(e), ", fd: ", fd);
    }
}

// kevent returns 0 if timedout
int Kqueue::wait(int ms) {
    if (ms >= 0) {
        struct timespec ts = { ms / 1000, ms % 1000 * 1000000 };
        _n = ::kevent(_kq, nullptr, 0, (Ev*)_events, N, &ts);
    } else {
        _n = ::kevent(_kq, nullptr, 0, (Ev*)_events, N, nullptr);
    }
    if (_n < 0) {
        const int e = errno;
        log::error("kqueue wait error: ", co::strerror(e), ", fd: ", _kq);
    }
    return _n;
}

void Kqueue::signal() {
    if (atomic_bool_cas(&_signaled, 0, 1, mo_relaxed, mo_relaxed)) {
        Ev ev;
        EV_SET(&ev, 0, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
        if (::kevent(_kq, &ev, 1, nullptr, 0, nullptr) < 0) {
            const int e = errno;
            log::error("kqueue EVFILT_USER trigger error: ", co::strerror(e));
            atomic_store(&_signaled, 0, mo_relaxed);
        }
    }
}

void Kqueue::handle_events() {
    auto events = (Ev*)_events;
    const int n = _n;
    _n = 0;
    for (int i = 0; i < n; ++i) {
        const auto ev = &events[i];
        if (ev->filter == EVFILT_USER) {
            atomic_store(&_signaled, 0, mo_relaxed);
            continue;
        }
        const auto co = (Coroutine*) ev->udata;
        if (co) co->sched->resume(co);
    }
}

} // co

#endif
