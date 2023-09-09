#include "hook.h"
#include "sched.h"

namespace co {

#ifndef _WIN32

io_event::~io_event() {
    if (_added) xx::gSched->del_io_event(_fd, _ev);
}

bool io_event::wait(uint32 ms) {
    auto sched = xx::gSched;
    if (!_added) {
        _added = sched->add_io_event(_fd, _ev);
        if (!_added) return false;
    }

    if (ms != (uint32)-1) {
        sched->add_timer(ms);
        sched->yield();
        if (!sched->timeout()) return true;
        errno = ETIMEDOUT;
        return false;
    } else {
        sched->yield();
        return true;
    }
}

#else
using xx::PerIoInfo;
extern bool can_skip_iocp_on_success;

io_event::io_event(sock_t fd, _ev_t ev)
    : _fd(fd), _to(0), _nb_tcp(ev == ev_read ? nb_tcp_recv : nb_tcp_send), _timeout(false) {
    const auto sched = xx::gSched;
    sched->add_io_event(fd, ev); // add socket to IOCP
    _info = (PerIoInfo*) co::alloc(sizeof(PerIoInfo), co::cache_line_size);
    memset(_info, 0, sizeof(PerIoInfo));
    _info->mlen = sizeof(PerIoInfo);
    _info->co = (void*) sched->running();
    sched->running()->waitx = (xx::waitx_t*)_info;
}

io_event::io_event(sock_t fd, int n)
    : _fd(fd), _to(0), _nb_tcp(0), _timeout(false) {
    const auto sched = xx::gSched;
    sched->add_io_event(fd, ev_read); // add socket to IOCP
    _info = (PerIoInfo*) co::alloc(sizeof(PerIoInfo) + n, co::cache_line_size);
    memset(_info, 0, sizeof(PerIoInfo) + n);
    _info->mlen = sizeof(PerIoInfo) + n;
    _info->co = (void*) sched->running();
    sched->running()->waitx = (xx::waitx_t*)_info;
}

io_event::io_event(sock_t fd, _ev_t ev, const void* buf, int size, int n)
    : _fd(fd), _to(0), _nb_tcp(0), _timeout(false) {
    const auto sched = xx::gSched;
    sched->add_io_event(fd, ev);
    if (!sched->on_stack(buf)) {
        _info = (PerIoInfo*) co::alloc(sizeof(PerIoInfo) + n, co::cache_line_size);
        memset(_info, 0, sizeof(PerIoInfo) + n);
        _info->mlen = sizeof(PerIoInfo) + n;
        _info->co = (void*) sched->running();
        _info->buf.buf = (char*)buf;
        _info->buf.len = size;
    } else {
        _info = (PerIoInfo*) co::alloc(sizeof(PerIoInfo) + n + size, co::cache_line_size);
        memset(_info, 0, sizeof(PerIoInfo) + n);
        _info->mlen = sizeof(PerIoInfo) + n + size;
        _info->co = (void*) sched->running();
        _info->buf.buf = _info->s + n;
        _info->buf.len = size;
        if (ev == ev_read) {
            _to = (char*)buf;
        } else {
            memcpy(_info->buf.buf, buf, size);
        }
    }
    sched->running()->waitx = (xx::waitx_t*)_info;
}

io_event::~io_event() {
    if (!_timeout) {
        if (_to && _info->n > 0) memcpy(_to, _info->buf.buf, _info->n);
        co::free(_info, _info->mlen);
    } 
    xx::gSched->running()->waitx = 0;
}

bool io_event::wait(uint32 ms) {
    int r, e;
    const auto sched = xx::gSched;
    if (_info->state != xx::st_wait) _info->state = xx::st_wait;

    // If fd is a non-blocking TCP socket, we post the I/O operation to IOCP using 
    // WSARecv or WSASend. Since _info->buf is empty, no data will be transfered, 
    // but we'll know that the socket is readable or writable when IOCP completes.
    if (_nb_tcp != 0) {
        if (_nb_tcp == nb_tcp_recv) {
            r = __sys_api(WSARecv)(_fd, &_info->buf, 1, &_info->n, &_info->flags, &_info->ol, 0);
            if (r == 0 && can_skip_iocp_on_success) return true;
            if (r == -1) {
                e = WSAGetLastError();
                if (e != WSA_IO_PENDING) goto err;
            }
        } else {
            r = __sys_api(WSASend)(_fd, &_info->buf, 1, &_info->n, 0, &_info->ol, 0);
            if (r == 0 && can_skip_iocp_on_success) return true;
            if (r == -1) {
                e = WSAGetLastError();
                if (e == WSAENOTCONN) goto wait_for_connect; // the socket is not connected yet
                if (e != WSA_IO_PENDING) goto err;
            }
        }
    }

    if (ms != (uint32)-1) {
        sched->add_timer(ms);
        sched->yield();
        _timeout = sched->timeout();
        if (!_timeout) return true;

        CancelIo((HANDLE)_fd);
        co::error(ETIMEDOUT);
        WSASetLastError(WSAETIMEDOUT);
        return false;
    } else {
        sched->yield();
        return true;
    }

  err:
    co::error(e);
    return false;

  wait_for_connect:
    {
        // as no IO operation was posted to IOCP, remove waitx from coroutine.
        sched->running()->waitx = 0;

        // check if the socket is connected every x ms
        uint32 x = 1;
        int r, sec = 0, len = sizeof(int);
        while (true) {
            r = co::getsockopt(_fd, SOL_SOCKET, SO_CONNECT_TIME, &sec, &len);
            if (r != 0) return false;
            if (sec >= 0) return true; // connect ok
            if (ms == 0) {
                co::error(ETIMEDOUT);
                WSASetLastError(WSAETIMEDOUT);
                return false;
            }
            sched->sleep(ms > x ? x : ms);
            if (ms != (uint32)-1) ms = (ms > x ? ms - x : 0);
            if (x < 16) x <<= 1;
        }
    }
}

#endif

} // co
