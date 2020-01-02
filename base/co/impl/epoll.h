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

#ifdef _WIN32
typedef OVERLAPPED_ENTRY epoll_event;

// single thread for each iocp
class Epoll {
  public:
    Epoll();
    ~Epoll();

    bool add_ev_read(sock_t fd) {
        return this->_Add_event(fd, 1);
    }

    bool add_ev_write(sock_t fd) {
        return this->_Add_event(fd, 2);
    }

    void del_ev_read(sock_t fd) {
        this->_Del_event(fd, 1);
    }

    void del_ev_write(sock_t fd) {
        this->_Del_event(fd, 2);
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
    bool _Add_event(sock_t fd, int ev);

    void _Del_event(sock_t fd, int ev) {
        auto it = _ev_map.find(fd);
        if (it != _ev_map.end() && (it->second & ev)) {
            it->second &= ~ev;
        }
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

    bool add_ev_read(int fd, int u);
    bool add_ev_write(int fd, int u);

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

    static bool has_ev_read(const epoll_event& ev) {
        return ev.events & EPOLLIN;
    }

    static bool has_ev_write(const epoll_event& ev) {
        return ev.events & EPOLLOUT;
    }

    static bool has_ev_error(const epoll_event& ev) {
        return ev.events & (EPOLLERR | EPOLLHUP);
    }

    static bool is_ev_pipe(const epoll_event& ev) {
        return ev.data.u64 == 0;
    }

    static uint32 ud(const epoll_event& ev) {
        const uint64 x = ev.data.u64;
        return ((uint32)x) == 0 ? ((uint32)(x >> 32)) : ((uint32)x);
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

    bool add_ev_read(int fd, void* p);
    bool add_ev_write(int fd, void* p);

    void del_ev_read(int fd);
    void del_ev_write(int fd);

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
