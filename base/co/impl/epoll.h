#pragma once

#include "../co.h"
#include "../../atomic.h"
#include "../../log.h"
#include "hook.h"
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

enum {
    EV_read = 1,
    EV_write =2,
};

#ifdef _WIN32
typedef OVERLAPPED_ENTRY epoll_event;

// single thread for each iocp
class Epoll {
  public:
    Epoll();
    ~Epoll();

    bool add_event(sock_t fd, int ev);

    void del_event(sock_t fd, int ev) {
        auto it = _ev_map.find(fd);
        if (it != _ev_map.end() && (it->second & ev)) it->second &= ~ev;
    }

    void del_event(sock_t fd) {
        _ev_map.erase(fd);
    }

    int wait(int ms) {
        ULONG n = 0;
        int r = GetQueuedCompletionStatusEx(_iocp, _ev, 1024, &n, ms, false);
        if (r == TRUE) return (int) n;
        return co::error() == WAIT_TIMEOUT ? 0 : -1;
    }

    const epoll_event& operator[](int i) const {
        return _ev[i];
    }

    void signal() {
        if (atomic_compare_swap(&_signaled, 0, 1) == 0) {
            if (PostQueuedCompletionStatus(_iocp, 0, 0, 0) == 0) {
                ELOG << "PostQueuedCompletionStatus error: " << co::strerror();
            }
        }
    }

    void handle_ev_pipe() {
        atomic_swap(&_signaled, false);
    }

    static bool is_ev_pipe(const epoll_event& ev) {
        return ev.lpOverlapped == 0;
    }

    static void* ud(const epoll_event& ev) {
        return ev.lpOverlapped;
    }

  private:
    HANDLE _iocp;
    epoll_event _ev[1024];
    std::unordered_map<sock_t, int> _ev_map;
    bool _signaled;
};

#else
#ifdef __linux__

class Epoll {
  public:
    Epoll();
    ~Epoll();

    bool add_event(int fd, int ev, int ud) {
        if (ev == EV_read) return this->add_ev_read(fd, ud);
        return this->add_ev_write(fd, ud);
    }

    void del_event(int fd, int ev) {
        if (ev == EV_read) return this->del_ev_read(fd);
        return this->del_ev_write(fd);
    }

    bool add_ev_read(int fd, int ud);
    bool add_ev_write(int fd, int ud);

    void del_ev_read(int fd);
    void del_ev_write(int fd);

    void del_event(int fd) {
        auto it = _ev_map.find(fd);
        if (it != _ev_map.end()) {
            _ev_map.erase(it);
            if (epoll_ctl(_efd, EPOLL_CTL_DEL, fd, (epoll_event*)8) != 0) {
                ELOG << "epoll del error: " << co::strerror() << ", fd: " << fd;
            }
        }
    }

    int wait(int ms) {
        return fp_epoll_wait(_efd, _ev, 1024, ms);
    }

    const epoll_event& operator[](int i) const {
        return _ev[i];
    }

    static bool is_ev_pipe(const epoll_event& ev) {
        return ev.data.u64 == 0;
    }

    // @ev.data.u64:
    //   higher 32 bits: id of read coroutine
    //    lower 32 bits: id of write coroutine
    // 
    //   IN  &   OUT  ->  ev.data.u64
    //  !IN  &  !OUT  ->  ev.data.u64         (EPOLLERR | EPOLLHUP)
    //   IN  &  !OUT  ->  ev.data.u64 >> 32   
    //  !IN  &   OUT  ->  ev.data.u64 << 32
    static uint64 ud(const epoll_event& ev) {
        if (ev.events & EPOLLIN) {
            return (ev.events & EPOLLOUT) ? ev.data.u64 : (ev.data.u64 >> 32);
        } else {
            return (ev.events & EPOLLOUT) ? (ev.data.u64 << 32) : ev.data.u64;
        }
    }

    void signal(char c = 'x') {
        if (atomic_compare_swap(&_signaled, 0, 1) == 0) {
            int r = (int) fp_write(_fds[1], &c, 1);
            ELOG_IF(r != 1) << "pipe write error..";
        }
    }

    void handle_ev_pipe();

  private:
    int _efd;
    int _fds[2]; // pipe fd
    epoll_event _ev[1024];
    std::unordered_map<int, uint64> _ev_map;
    bool _signaled;
};

#else // kqueue
typedef struct kevent epoll_event;

class Epoll {
  public:
    Epoll();
    ~Epoll();

    bool add_event(int fd, int ev, void* ud);

    void del_event(int fd, int ev);

    void del_event(int fd);

    int wait(int ms) {
        if (ms >= 0) {
            struct timespec ts = { ms / 1000, ms % 1000 * 1000000 };
            return fp_kevent(_kq, 0, 0, _ev, 1024, &ts);
        } else {
            return fp_kevent(_kq, 0, 0, _ev, 1024, 0);
        }
    }

    const epoll_event& operator[](int i) const {
        return _ev[i];
    }

    static bool is_ev_pipe(const epoll_event& ev) {
        return ev.udata == 0;
    }

    static void* ud(const epoll_event& ev) {
        return ev.udata;
    }

    void signal(char c = 'x') {
        if (atomic_compare_swap(&_signaled, 0, 1) == 0) {
            int r = (int) fp_write(_fds[1], &c, 1);
            ELOG_IF(r != 1) << "pipe write error..";
        }
    }

    void handle_ev_pipe();

  private:
    int _kq;
    int _fds[2]; // pipe fd
    epoll_event _ev[1024];
    std::unordered_map<int, int> _ev_map;
    bool _signaled;
};

#endif // kqueue
#endif // iocp

} // co
