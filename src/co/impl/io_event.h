#pragma once

#include "scheduler.h"

namespace co {

#ifdef _WIN32
class IoEvent {
  public:
    IoEvent(sock_t fd)
        : _fd(fd), _id(null_timer_id) {
        gSched->add_event(fd);
    }

    ~IoEvent() {
        if (_id != null_timer_id) gSched->del_timer(_id);
    }

    bool wait(int ms=-1) {
        if (ms < 0) { gSched->yield(); return true; }
        
        if (_id == null_timer_id) _id = gSched->add_io_timer(ms);
        gSched->yield();
        if (!gSched->timeout()) return true;

        ELOG_IF(!CancelIo((HANDLE)_fd)) << "cancel io for fd " << _fd << " failed..";
        _id = null_timer_id;
        WSASetLastError(ETIMEDOUT);
        return false;
    }

  private:
    sock_t _fd;
    timer_id_t _id;
};

#else
class IoEvent {
  public:
    IoEvent(sock_t fd, int ev)
        : _fd(fd), _ev(ev), _id(null_timer_id), _has_ev(false) {
    }

    ~IoEvent() {
        if (_has_ev) gSched->del_event(_fd, _ev);
        if (_id != null_timer_id) {
            COLOG << "IoEvent del io_timer: " << _id << " fd: " << _fd;
            gSched->del_timer(_id);
        }
    }

    bool wait(int ms=-1) {
        if (!_has_ev) _has_ev = gSched->add_event(_fd, _ev);
        if (ms < 0) { gSched->yield(); return true; }

        if (_id == null_timer_id) {
            _id = gSched->add_io_timer(ms);
            COLOG << " IoEvent add io_timer: " << _id << " fd: " << _fd;
        }
        gSched->yield();
        if (!gSched->timeout()) return true;

        _id = null_timer_id;
        errno = ETIMEDOUT;
        return false;
    }

  private:
    sock_t _fd;
    int _ev;
    timer_id_t _id;
    bool _has_ev;
};
#endif

} // co
