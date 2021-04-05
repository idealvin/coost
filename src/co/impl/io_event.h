#pragma once

#include "scheduler.h"

namespace co {

#ifdef _WIN32
class IoEvent {
  public:
    // We don't care what kind of event it is on Windows.
    IoEvent(sock_t fd)
        : _fd(fd), _has_timer(false) {
        gSched->add_event(fd);
    }

    // We needn't delete the event on windows.
    ~IoEvent() = default;

    bool wait(int ms=-1) {
        if (ms < 0) { gSched->yield(); return true; }
        
        if (!_has_timer) { _has_timer = true; gSched->add_io_timer(ms); }
        gSched->yield();
        if (!gSched->timeout()) return true;

        CancelIo((HANDLE)_fd);
        WSASetLastError(ETIMEDOUT);
        return false;
    }

  private:
    sock_t _fd;
    bool _has_timer;
};

#else
class IoEvent {
  public:
    IoEvent(sock_t fd, int ev)
        : _fd(fd), _ev(ev), _has_timer(false), _has_ev(false) {
    }

    ~IoEvent() {
        if (_has_ev) gSched->del_event(_fd, _ev);
    }

    bool wait(int ms=-1) {
        if (!_has_ev) _has_ev = gSched->add_event(_fd, _ev);
        if (ms < 0) { gSched->yield(); return true; }

        if (!_has_timer) { _has_timer = true; gSched->add_io_timer(ms); }
        gSched->yield();
        if (!gSched->timeout()) return true;

        errno = ETIMEDOUT;
        return false;
    }

  private:
    sock_t _fd;
    int _ev;
    bool _has_timer;
    bool _has_ev;
};
#endif

} // co
