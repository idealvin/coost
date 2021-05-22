#ifdef _WIN32
#include "co/co/io_event.h"

namespace co {

bool IoEvent::wait(int ms) {
    if (_ev == EV_read) {
        int r = WSARecv(_fd, &_info->buf, 1, &_info->n, &_info->flags, &_info->ol, 0);
        if (r == -1 && co::error() != WSA_IO_PENDING) return false;
    } else if (_ev == EV_write) {
        int r = WSASend(_fd, &_info->buf, 1, &_info->n, 0, &_info->ol, 0);
        if (r == -1) {
            do {
                const int err = co::error();
                if (err == WSA_IO_PENDING) break;
                if (err == WSAENOTCONN) { /* the socket is not connected yet */
                    int sec = 0, len = sizeof(sec);
                    while (true) {
                        gSched->add_io_timer(16);
                        gSched->yield();
                        if (ms >= 0 && ((ms -= 16) < 0)) {
                            co::set_last_error(ETIMEDOUT);
                            return false;
                        }

                        r = co::getsockopt(_fd, SOL_SOCKET, SO_CONNECT_TIME, &sec, &len);
                        if (r == 0) {
                            if (sec >= 0) return true;
                        } else {
                            return false;
                        }
                    }
                } else {
                    return false;
                }
            } while (0);
        }
    }

    if (ms >= 0) {
        gSched->add_io_timer(ms);
        gSched->yield();
        if (!gSched->timeout()) return true;
        CancelIo((HANDLE)_fd);
        co::set_last_error(ETIMEDOUT);
        _info->co = 0;
        return false;
    } else {
        gSched->yield();
        return true;
    }
}

} // co

#endif
