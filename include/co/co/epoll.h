#pragma once

#include "sock.h"
#include "hook.h"
#include "../atomic.h"
#include "../log.h"

#include <vector>
#include <unordered_map>

#ifndef _WIN32
#ifdef __linux__
#include <sys/epoll.h>
#else
#include <time.h>
#include <sys/event.h>
#endif
#endif

namespace co {

enum io_event_t {
    EV_read = 1,
    EV_write = 2,
};

#ifdef _WIN32
typedef OVERLAPPED_ENTRY epoll_event;

class Epoll {
  public:
    Epoll() : _ev(1024), _signaled(false) {
        _iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
        CHECK(_iocp != 0) << "create iocp failed..";
    }

    ~Epoll() { this->close(); }

    bool add_event(sock_t fd) {
        int& x = _ev_map[fd];
        if (x) return true;
        if (CreateIoCompletionPort((HANDLE)fd, _iocp, fd, 1) != 0) {
            x = EV_read | EV_write;
            return true;
        } else {
            ELOG << "iocp add fd " << fd << " error: " << co::strerror();
            return false;
        }
    }

    void del_event(sock_t fd) { _ev_map.erase(fd); }

    void del_event(sock_t fd, io_event_t ev) {
        auto it = _ev_map.find(fd);
        if (it != _ev_map.end() && it->second & ev) it->second &= ~ev;
    }

    int wait(int ms) {
        ULONG n = 0;
        const BOOL r = GetQueuedCompletionStatusEx(_iocp, _ev.data(), 1024, &n, ms, false);
        if (r == TRUE) return (int)n;
        return co::error() == WAIT_TIMEOUT ? 0 : -1;
    }

    void signal() {
        if (atomic_compare_swap(&_signaled, false, true) == false) {
            const BOOL r = PostQueuedCompletionStatus(_iocp, 0, 0, 0);
            ELOG_IF(!r) << "PostQueuedCompletionStatus error: " << co::strerror();
        }
    }

    void close() {
        if (_iocp) { CloseHandle(_iocp); _iocp = 0; }
    }

    const epoll_event& operator[](int i)    const { return _ev[i]; }
    static void* user_data(const epoll_event& ev) { return ev.lpOverlapped; }
    static bool is_ev_pipe(const epoll_event& ev) { return ev.lpOverlapped == 0; }
    void handle_ev_pipe() { atomic_swap(&_signaled, false); }

  private:
    HANDLE _iocp;
    std::vector<epoll_event> _ev;
    std::unordered_map<sock_t, int> _ev_map;
    bool _signaled;
};

#else
#ifdef __linux__
/**
 * Epoll for Linux 
 *   - We have to consider about that two different coroutines works on a same 
 *     socket, one for read and one for write. 
 * 
 *     That is not a problem on windows & mac, as in IOCP or kqueue, we set a 
 *     different user data for each IO. For example, we may set a pointer to 
 *     coroutine A as the user data for EV_read, and a pointer to coroutine B 
 *     as the user data for EV_write. When EV_read is present, coroutine A will 
 *     be resumed, and when EV_write is present, coroutine B will be resumed, 
 *     which works perfectly. 
 * 
 *     However, epoll works in a different way, in which, EV_read and EV_write 
 *     share a same user data. To solve the above problem, we use data.u64 of 
 *     epoll_event to store the user data: 
 *       - the higher 32 bits:  id of the coroutine waiting for EV_read. 
 *       - the lower  32 bits:  id of the coroutine waiting for EV_write. 
 * 
 *     When an IO event is present, id in the user data will be used to resume 
 *     the corresponding coroutine.
 */
class Epoll {
  public:
    Epoll();
    ~Epoll() { this->close(); }

    bool add_event(int fd, io_event_t ev, int32 ud) {
        return (ev == EV_read) ? add_ev_read(fd, ud) : add_ev_write(fd, ud);
    }

    void del_event(int fd, io_event_t ev) {
        (ev == EV_read) ? del_ev_read(fd) : del_ev_write(fd);
    }

    void del_event(int fd) {
        auto it = _ev_map.find(fd);
        if (it != _ev_map.end()) {
            _ev_map.erase(it);
            const int r = epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, (epoll_event*)8);
            if (r != 0) ELOG << "epoll del error: " << co::strerror() << ", fd: " << fd;
        }
    }

    int wait(int ms) {
        return raw_epoll_wait(_epoll_fd, _ev.data(), 1024, ms);
    }

    void signal(char c = 'x') {
        if (atomic_compare_swap(&_signaled, 0, 1) == 0) {
            const int r = (int) raw_write(_pipe_fds[1], &c, 1);
            ELOG_IF(r != 1) << "pipe write error..";
        }
    }

    const epoll_event& operator[](int i)    const { return _ev[i]; }
    static bool is_ev_pipe(const epoll_event& ev) { return ev.data.u64 == 0; }

    static uint64 user_data(const epoll_event& ev) {
        if (ev.events & EPOLLIN) {
            return (ev.events & EPOLLOUT) ? ev.data.u64 : (ev.data.u64 >> 32);
        } else {
            return (ev.events & EPOLLOUT) ? (ev.data.u64 << 32) : ev.data.u64;
        }
    }

    void handle_ev_pipe();
    void close();

  private:
    bool add_ev_read(int fd, int32 ud);
    bool add_ev_write(int fd, int32 ud);
    void del_ev_read(int fd);
    void del_ev_write(int fd);

  private:
    int _epoll_fd;
    int _pipe_fds[2];
    int _signaled;
    std::vector<epoll_event> _ev;
    std::unordered_map<int, uint64> _ev_map;
};

#else // kqueue
typedef struct kevent epoll_event;

class Epoll {
  public:
    Epoll();
    ~Epoll() { this->close(); }

    bool add_event(int fd, io_event_t ev, void* ud);
    void del_event(int fd, io_event_t ev);
    void del_event(int fd);

    int wait(int ms) {
        if (ms >= 0) {
            struct timespec ts = { ms / 1000, ms % 1000 * 1000000 };
            return raw_kevent(_epoll_fd, 0, 0, _ev.data(), 1024, &ts);
        } else {
            return raw_kevent(_epoll_fd, 0, 0, _ev.data(), 1024, 0);
        }
    }

    void signal(char c = 'x') {
        if (atomic_compare_swap(&_signaled, 0, 1) == 0) {
            const int r = (int) raw_write(_pipe_fds[1], &c, 1);
            ELOG_IF(r != 1) << "pipe write error..";
        }
    }

    const epoll_event& operator[](int i)    const { return _ev[i]; }
    static bool is_ev_pipe(const epoll_event& ev) { return ev.udata == 0; }
    static void* user_data(const epoll_event& ev) { return ev.udata; }
    void handle_ev_pipe();
    void close();

  private:
    int _epoll_fd;
    int _pipe_fds[2];
    int _signaled;
    std::vector<epoll_event> _ev;
    std::unordered_map<int, int> _ev_map;
};

#endif // kqueue
#endif // iocp

} // co
