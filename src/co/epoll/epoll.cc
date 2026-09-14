#ifdef __linux__
#include "epoll.h"
#include "co/error.h"
#include "co/log.h"
#include "../sched.h"
#include "../../close.h"

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

namespace co {

using Ev = struct epoll_event;

Epoll::Epoll() : _signaled(0), _n(0) {
    _ep = ::epoll_create1(EPOLL_CLOEXEC);
    runtime_assert(_ep != -1);

    _efd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    runtime_assert(_efd != -1);
    runtime_assert(this->add_ev_read(_efd, nullptr));

    _events = (Ev*) co::_static_alloc(N * sizeof(Ev), co::cache_line_size);
    ::memset(_events, 0, N * sizeof(Ev));
}

Epoll::~Epoll() {
    _close(_efd);
    _close(_ep);
}

bool Epoll::add_ev_read(sock_t fd, void* c) {
    Ev ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = c;

    const int r = ::epoll_ctl(_ep, EPOLL_CTL_ADD, fd, &ev);
    if (r == 0) return true;

    const int e = errno;
    runtime_assert(e != EEXIST, "Coroutines concurrently reading/writing a socket?")
    log::error("epoll add ev_read error: ", co::strerror(e), ", fd: ", fd, " co: ", c);
    return false;
}

bool Epoll::add_ev_write(sock_t fd, void* c) {
    Ev ev;
    ev.events = EPOLLOUT | EPOLLET;
    ev.data.ptr = c;

    const int r = ::epoll_ctl(_ep, EPOLL_CTL_ADD, fd, &ev);
    if (r == 0) return true;

    const int e = errno;
    runtime_assert(e != EEXIST, "Coroutines concurrently reading/writing a socket?")
    log::error("epoll add ev_write error: ", co::strerror(e), ", fd: ", fd, " co: ", c);
    return false;
}

void Epoll::del_event(sock_t fd) {
    const int r = ::epoll_ctl(_ep, EPOLL_CTL_DEL, fd, (Ev*)128);
    if (r != 0 && errno != ENOENT) {
        const int e = errno;
        log::error("epoll del event error: ", co::strerror(e), ", fd: ", fd);
    }
}

// epoll_wait returns 0 if timedout
int Epoll::wait(int ms) {
    _n = ::epoll_wait(_ep, (Ev*)_events, N, ms);
    if (_n < 0) {
        const int e = errno;
        log::error("epoll wait error: ", co::strerror(e), ", fd: ", _ep);
    }
    return _n;
}

void Epoll::signal() {
    if (atomic_bool_cas(&_signaled, 0, 1, mo_relaxed, mo_relaxed)) {
        // add 1 to the internal counter of eventfd
        uint64 v = 1;
        while (true) {
            const auto r = ::write(_efd, &v, sizeof(v));
            if (r > 0) break;
            const int e = errno;
            if (e == EINTR) continue;
            log::error("eventfd write error: ", co::strerror(e), ", fd: ", _efd);
            atomic_store(&_signaled, 0, mo_relaxed);
        }
    }
}

void Epoll::handle_events() {
    auto events = (Ev*)_events;
    const int n = _n;
    _n = 0;
    for (int i = 0; i < n; ++i) {
        const auto co = (Coroutine*) events[i].data.ptr;
        if (co) {
            co->sched->resume(co);
        } else {
            // reset the internal counter of eventfd to 0
            uint64 v;
            while (true) {
                const auto r = ::read(_efd, &v, sizeof(v));
                if (__unlikely(r < 0)) {
                    const int e = errno;
                    if (e == EINTR) continue;
                    log::error("eventfd read error: ", co::strerror(e), " fd: ", _efd);
                }
                atomic_store(&_signaled, 0, mo_relaxed);
                break;
            }
        }
    }
}

} // co

#endif
