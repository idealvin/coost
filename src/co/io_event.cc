#include "hook.h"
#include "scheduler.h"

namespace co {

extern bool can_skip_iocp_on_success;

#ifdef _WIN32
IoEvent::IoEvent(sock_t fd, io_event_t ev)
    : _fd(fd), _to(0), _nb_tcp(ev == ev_read ? nb_tcp_recv : nb_tcp_send), _timeout(false) {
    auto s = gSched;
    s->add_io_event(fd, ev); // add socket to IOCP
    _info = (PerIoInfo*) calloc(1, sizeof(PerIoInfo));
    _info->co = (void*) s->running();
    s->running()->waitx = _info;
}

IoEvent::IoEvent(sock_t fd, int n)
    : _fd(fd), _to(0), _nb_tcp(0), _timeout(false) {
    auto s = gSched;
    s->add_io_event(fd, ev_read); // add socket to IOCP
    _info = (PerIoInfo*) calloc(1, sizeof(PerIoInfo) + n);
    _info->co = (void*) s->running();
    s->running()->waitx = _info;
}

IoEvent::IoEvent(sock_t fd, io_event_t ev, const void* buf, int size, int n)
    : _fd(fd), _to(0), _nb_tcp(0), _timeout(false) {
    auto s = gSched;
    s->add_io_event(fd, ev);
    if (!s->on_stack(buf)) {
        _info = (PerIoInfo*) calloc(1, sizeof(PerIoInfo) + n);
        _info->co = (void*) s->running();
        _info->buf.buf = (char*)buf;
        _info->buf.len = size;
    } else {
        _info = (PerIoInfo*) calloc(1, sizeof(PerIoInfo) + n + size);
        _info->co = (void*) s->running();
        _info->buf.buf = _info->s + n;
        _info->buf.len = size;
        if (ev == ev_read) {
            _to = (char*)buf;
        } else {
            memcpy(_info->buf.buf, buf, size);
        }
    }
    s->running()->waitx = _info;
}

IoEvent::~IoEvent() {
    if (!_timeout) {
        if (_to && _info->n > 0) memcpy(_to, _info->buf.buf, _info->n);
        ::free(_info);
    } 
    gSched->running()->waitx = 0;
}

bool IoEvent::wait(uint32 ms) {
    auto s = gSched;

    // If fd is a non-blocking TCP socket, we'll post an I/O operation to IOCP using 
    // WSARecv or WSASend. Since _info->buf is empty, no data will be transfered, but 
    // we should know that the socket is readable or writable when the IOCP completes.
    if (_nb_tcp != 0) {
        if (_nb_tcp == nb_tcp_recv) {
            int r = CO_RAW_API(WSARecv)(_fd, &_info->buf, 1, &_info->n, &_info->flags, &_info->ol, 0);
            if (r == 0 && can_skip_iocp_on_success) return true;
            if (r == -1 && co::error() != WSA_IO_PENDING) return false;
        } else {
            int r = CO_RAW_API(WSASend)(_fd, &_info->buf, 1, &_info->n, 0, &_info->ol, 0);
            if (r == 0 && can_skip_iocp_on_success) return true;
            if (r == -1) {
                const int e = co::error();
                if (e == WSAENOTCONN) goto wait_for_connect; // the socket is not connected yet
                if (e != WSA_IO_PENDING) return false;
            }
        }
    }

    if (ms != (uint32)-1) {
        s->add_timer(ms);
        s->yield();
        _timeout = s->timeout();
        if (!_timeout) {
            return true;
        } else {
            CancelIo((HANDLE)_fd);
            WSASetLastError(WSAETIMEDOUT);
            return false;
        }
    } else {
        s->yield();
        return true;
    }

  wait_for_connect:
    {
        // as no IO operation was posted to IOCP, remove ioinfo from coroutine.
        gSched->running()->waitx = 0;

        // check whether the socket is connected or not every 16 ms.
        uint32 t = 16;
        int r, sec = 0, len = sizeof(sec);
        while (true) {
            r = co::getsockopt(_fd, SOL_SOCKET, SO_CONNECT_TIME, &sec, &len);
            if (r != 0) return false;
            if (sec != -1) return true; // connect ok
            if (ms == 0) { WSASetLastError(WSAETIMEDOUT); return false; }
            if (ms < t) t = ms;
            s->sleep(t); // sleep for t ms in this coroutine
            if (ms != (uint32)-1) ms -= t;
            if (t < 64) t <<= 1;
        }
    }
}

#else
IoEvent::~IoEvent() {
    if (_has_ev) gSched->del_io_event(_fd, _ev);
}

bool IoEvent::wait(uint32 ms) {
    auto s = gSched;
    if (!_has_ev) {
        _has_ev = s->add_io_event(_fd, _ev);
        if (!_has_ev) return false;
    }

    if (ms != (uint32)-1) {
        s->add_timer(ms);
        s->yield();
        if (!s->timeout()) {
            return true;
        } else {
            errno = ETIMEDOUT;
            return false;
        }
    } else {
        s->yield();
        return true;
    }
}
#endif

} // co
