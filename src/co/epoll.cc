#ifndef _WIN32
#include "co/co.h"

namespace co {

void Epoll::handle_ev_pipe() {
    int32 dummy;
    while (true) {
        int r = raw_read(_pipe_fds[0], &dummy, 4);
        if (r != -1) {
            if (r < 4) break;
            continue;
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) break;
            if (errno == EINTR) continue;
            ELOG << "pipe read error: " << co::strerror() << ", fd: " << _pipe_fds[0];
            break;
        }
    }
    atomic_swap(&_signaled, 0);
}

inline void closesocket(int& fd) {
    if (fd != -1) {
        while (raw_close(fd) != 0 && errno == EINTR);
        fd = -1;
    }
}

void Epoll::close() {
    co::closesocket(_epoll_fd);
    co::closesocket(_pipe_fds[0]);
    co::closesocket(_pipe_fds[1]);
}

#ifdef __linux__
Epoll::Epoll() : _signaled(0), _ev(1024) {
    _epoll_fd = epoll_create(1024);
    CHECK_NE(_epoll_fd, -1) << "epoll create error: " << co::strerror();
    co::set_cloexec(_epoll_fd);

    CHECK_NE(::pipe(_pipe_fds), -1) << "create pipe error: " << co::strerror();
    co::set_cloexec(_pipe_fds[0]);
    co::set_cloexec(_pipe_fds[1]);
    co::set_nonblock(_pipe_fds[0]);
    CHECK(this->add_ev_read(_pipe_fds[0], 0));
}

bool Epoll::add_ev_read(int fd, int32 ud) {
    auto& x = _ev_map[fd];
    if (x >> 32) return true; // already exists

    epoll_event event;
    event.events = x ? (EPOLLIN | EPOLLOUT | EPOLLET) : (EPOLLIN | EPOLLET);
    event.data.u64 = ((uint64)ud << 32) | x;

    const int r = epoll_ctl(_epoll_fd, x ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, fd, &event);
    if (r == 0) { x = event.data.u64; return true; }

    ELOG << "add ev read error: " << co::strerror() << ", fd: " << fd << ", co: " << ud;
    return false;
}

bool Epoll::add_ev_write(int fd, int32 ud) {
    auto& x = _ev_map[fd];
    if ((uint32)x) return true; // already exists

    epoll_event event;
    event.events = x ? (EPOLLIN | EPOLLOUT | EPOLLET) : (EPOLLOUT | EPOLLET);
    event.data.u64 = x | ud;

    const int r = epoll_ctl(_epoll_fd, x ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, fd, &event);
    if (r == 0) { x = event.data.u64; return true; }

    ELOG << "add ev write error: " << co::strerror() << ", fd: " << fd << ", co: " << ud;
    return false;
}

void Epoll::del_ev_read(int fd) {
    auto it = _ev_map.find(fd);
    if (it == _ev_map.end() || !(it->second >> 32)) return; // not exists

    int r;
    if (!(uint32)it->second) {
        _ev_map.erase(it);
        r = epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, (epoll_event*)8);
    } else {
        it->second = (uint32)it->second;
        epoll_event event;
        event.events = EPOLLOUT | EPOLLET;
        event.data.u64 = it->second;
        r = epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &event);
    }

    ELOG_IF(r != 0) << "del ev read error: " << co::strerror() << ", fd: " << fd;
}

void Epoll::del_ev_write(int fd) {
    auto it = _ev_map.find(fd);
    if (it == _ev_map.end() || !(uint32)it->second) return; // not exists

    int r;
    if (!(it->second >> 32)) {
        _ev_map.erase(it);
        r = epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, (epoll_event*)8);
    } else {
        it->second &= ((uint64)-1 << 32);
        epoll_event event;
        event.events = EPOLLIN | EPOLLET;
        event.data.u64 = it->second;
        r = epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &event);
    }

    ELOG_IF(r != 0) << "del ev write error: " << co::strerror() << ", fd: " << fd;
}

#else  /* kqueue */
Epoll::Epoll() : _signaled(0), _ev(1024) {
    _epoll_fd = kqueue();
    CHECK_NE(_epoll_fd, -1) << "kqueue create error: " << co::strerror();

    CHECK_NE(::pipe(_pipe_fds), -1) << "create pipe error: " << co::strerror();
    co::set_cloexec(_pipe_fds[0]);
    co::set_cloexec(_pipe_fds[1]);
    co::set_nonblock(_pipe_fds[0]);
    CHECK(this->add_event(_pipe_fds[0], EV_read, 0));
}

bool Epoll::add_event(int fd, io_event_t ev, void* p) {
    int& x = _ev_map[fd];
    if (x & ev) return true; // already exists

    struct kevent event;
    const int evfilt = (ev == EV_read ? EVFILT_READ : EVFILT_WRITE);
    EV_SET(&event, fd, evfilt, EV_ADD, 0, 0, p);

    if (raw_kevent(_epoll_fd, &event, 1, 0, 0, 0) == 0) {
        x |= ev;
        return true;
    } else {
        ELOG << "kqueue add event error: " << co::strerror() << ", fd: " << fd << ", ev: " << ev;
        return false;
    }
}

void Epoll::del_event(int fd, io_event_t ev) {
    auto it = _ev_map.find(fd);
    if (it == _ev_map.end() || !(it->second & ev)) return;

    int& x = it->second;
    x == ev ? (void) _ev_map.erase(it) : (void) (x &= ~ev);

    struct kevent event;
    const int evfilt = (ev == EV_read ? EVFILT_READ : EVFILT_WRITE);
    EV_SET(&event, fd, evfilt, EV_DELETE, 0, 0, 0);

    if (raw_kevent(_epoll_fd, &event, 1, 0, 0, 0) != 0) {
        ELOG << "kqueue del event error: " << co::strerror() << ", fd: " << fd << ", ev: " << ev;
    }
}

void Epoll::del_event(int fd) {
    auto it = _ev_map.find(fd);
    if (it == _ev_map.end()) return;

    int ev = it->second;
    _ev_map.erase(it);

    int i = 0;
    struct kevent event[2];
    if (ev & EV_read) EV_SET(&event[i++], fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
    if (ev & EV_write) EV_SET(&event[i++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);

    if (raw_kevent(_epoll_fd, event, i, 0, 0, 0) != 0) {
        ELOG << "kqueue del event error: " << co::strerror() << ", fd: " << fd;
    }
}
#endif

} // co

#endif
