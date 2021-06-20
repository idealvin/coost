#pragma once

#include "sock.h"
#include "scheduler.h"

namespace co {

#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning (disable:4200)
#endif

/**
 * IoEvent is for waiting an IO event on a socket 
 *   - It MUST be used in a coroutine. 
 *   - The socket MUST be non-blocking and overlapped on windows.
 *   - On windows, we MUST add a socket to IOCP before we call APIs like WSARecv. 
 */
class IoEvent {
  public:
    struct PerIoInfo {
        PerIoInfo() {
            memset(this, 0, sizeof(*this));
            co = xx::scheduler()->running();
        }

        WSAOVERLAPPED ol;
        void* co;     // user data, pointer to a coroutine
        DWORD n;      // bytes transfered
        DWORD flags;  // flags for WSARecv, WSARecvFrom
        WSABUF buf;   // buffer for WSARecv, WSARecvFrom, WSASend, WSASendTo
        char s[];     // extra buffer allocated
    };

    /**
     * the constructor 
     *   - In this case, no IO event is specified. The user MUST post an IO task 
     *     to IOCP manually before the wait() is called.
     *   - If n > 0, an extra n-byte buffer will be allocated with the PerIoInfo, 
     *     the user can use IoEvent->s to access the extra buffer later. 
     * 
     * @param fd  the socket.
     * @param n   extra bytes to be allocated for PerIoInfo.
     */
    IoEvent(sock_t fd, int n=0)
        : _fd(fd), _ev(0) {
        xx::scheduler()->add_io_event(fd, EV_read);
        _info = new (malloc(sizeof(PerIoInfo) + n)) PerIoInfo();
    }

    /**
     * the constructor 
     *   - In this case, an IO task will be added to IOCP at the beginning of wait(). 
     * 
     * @param fd  the socket.
     * @param ev  the IO event, either EV_read or EV_write.
     */
    IoEvent(sock_t fd, io_event_t ev)
        : _fd(fd), _ev(ev) {
        xx::scheduler()->add_io_event(fd, ev);
        _info = new (malloc(sizeof(PerIoInfo))) PerIoInfo();
    }

    ~IoEvent() {
        if (_info->co) {
            _info->~PerIoInfo();
            free(_info);
        }
    }

    PerIoInfo* operator->() const {
        return _info;
    }

    /**
     * wait for an IO event on a socket 
     *   - If _ev is EV_read or EV_write, WSARecv or WSASend will be called to 
     *     post an IO task to IOCP. As the buffer in PerIoInfo is NULL, no data 
     *     will be copied by IOCP. We just need IOCP to notify us that the socket 
     *     is readable or writable.
     *   - The errno will be set to ETIMEDOUT on timeout. The user can call 
     *     co::error() to get the errno later. 
     * 
     * @param ms  timeout in milliseconds, if ms < 0, it will never timeout. 
     *            default: -1. 
     * 
     * @return    true if an IO event is present, or false on timeout or error.
     */
    bool wait(int ms=-1);

  private:
    sock_t _fd;
    int _ev;
    PerIoInfo* _info;
};

#else
/**
 * IoEvent is for waiting an IO event on a socket 
 *   - It MUST be used in a coroutine. 
 *   - The socket MUST be non-blocking on Linux & Mac. 
 */
class IoEvent {
  public:
    /**
     * the constructor 
     * 
     * @param fd  the socket.
     * @param ev  the IO event, either EV_read or EV_write.
     */
    IoEvent(sock_t fd, io_event_t ev)
        : _fd(fd), _ev(ev), _has_ev(false) {
    }

    ~IoEvent() {
        if (_has_ev) xx::scheduler()->del_io_event(_fd, _ev);
    }

    /**
     * wait for an IO event on a socket 
     *   - The errno will be set to ETIMEDOUT on timeout. The user can call 
     *     co::error() to get the errno later. 
     * 
     * @param ms  timeout in milliseconds, if ms < 0, it will never timeout. 
     *            default: -1. 
     * 
     * @return    true if an IO event is present, or false on timeout or error.
     */
    bool wait(int ms=-1) {
        if (!_has_ev) {
            _has_ev = xx::scheduler()->add_io_event(_fd, _ev);
            if (!_has_ev) return false;
        }

        if (ms >= 0) {
            xx::scheduler()->add_io_timer(ms);
            xx::scheduler()->yield();
            if (!xx::scheduler()->timeout()) {
                return true;
            } else {
                errno = ETIMEDOUT;
                return false;
            }
        } else {
            xx::scheduler()->yield();
            return true;
        }
    }

  private:
    sock_t _fd;
    io_event_t _ev;
    bool _has_ev;
};
#endif

} // co
