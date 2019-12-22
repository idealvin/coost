#include "epoll.h"

namespace co {

#ifdef _WIN32
Epoll::Epoll() : _signaled(false) {
    _iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
    CHECK(_iocp != 0) << "create iocp failed..";
}

Epoll::~Epoll() {
    if (_iocp != 0) {
        CloseHandle(_iocp);
        _iocp = 0;
    }
}

bool Epoll::_Add_event(sock_t fd, int ev) {
    int& x = _ev_map[fd];
    if (x != 0) {
        if (!(x & ev)) x |= ev;
        return true; // already exists
    } else {
        if (CreateIoCompletionPort((HANDLE)fd, _iocp, fd, 1) != 0) {
            x = ev | 4;
            return true;
        }
        ELOG << "iocp add fd " << fd << " failed: " << GetLastError() << " " << co::strerror();
        return false;
    }
}

#else

void Epoll::handle_ev_pipe() {
    int dummy;
    while (true) {
        int r = fp_read(_fds[0], &dummy, 4);
        if (r != -1) {
            if (r < 4) break;
            continue;
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) break;
            if (errno == EINTR) continue;
            ELOG << "pipe read error: " << co::strerror() << ", fd: " << _fds[0];
            break;
        }
    }
    atomic_swap(&_signaled, false);
}

inline void closesocket(sock_t& fd) {
    if (fd != -1) {
        while (fp_close(fd) != 0 && errno == EINTR);
        fd = -1;
    }
}

#ifdef __linux__
Epoll::Epoll() {
    _efd = epoll_create(1024);
    CHECK_NE(_efd, -1) << "epoll create error: " << co::strerror();
    co::set_cloexec(_efd);

    CHECK_NE(::pipe(_fds), -1) << "create pipe error: " << co::strerror();
    co::set_cloexec(_fds[0]);
    co::set_cloexec(_fds[1]);
    co::set_nonblock(_fds[0]);
    CHECK(this->add_ev_read(_fds[0], 0));
}

Epoll::~Epoll() {
    co::closesocket(_efd);
    co::closesocket(_fds[0]);
    co::closesocket(_fds[1]);
}

bool Epoll::add_ev_read(int fd, int u) {
    auto& x = _ev_map[fd];
    if (x >> 32) return true; // already exists

    epoll_event event;
    event.events = x ? (EPOLLIN | EPOLLOUT | EPOLLET) : (EPOLLIN | EPOLLET);
    event.data.u64 = ((uint64)u << 32) | x;

    if (epoll_ctl(_efd, EPOLL_CTL_ADD, fd, &event) == 0) {
        x = event.data.u64;
        return true;
    }

    ELOG << "epoll add ev_read error: " << co::strerror() << ", fd: " << fd;
    return false;
}

bool Epoll::add_ev_write(int fd, int u) {
    auto& x = _ev_map[fd];
    if ((uint32)x) return true; // already exists

    epoll_event event;
    event.events = x ? (EPOLLIN | EPOLLOUT | EPOLLET) : (EPOLLOUT | EPOLLET);
    event.data.u64 = x | u;

    if (epoll_ctl(_efd, EPOLL_CTL_ADD, fd, &event) == 0) {
        x = event.data.u64;
        return true;
    }

    ELOG << "epoll add ev_write error: " << co::strerror() << ", fd: " << fd;
    return false;
}

void Epoll::del_ev_read(int fd) {
    auto it = _ev_map.find(fd);
    if (it == _ev_map.end() || !(it->second >> 32)) return;

    int r;
    if (!(uint32)it->second) {
        _ev_map.erase(it);
        r = epoll_ctl(_efd, EPOLL_CTL_DEL, fd, (epoll_event*)8);
    } else {
        it->second = (uint32)it->second;
        epoll_event event;
        event.events = EPOLLOUT | EPOLLET;
        event.data.u64 = it->second;
        r = epoll_ctl(_efd, EPOLL_CTL_MOD, fd, &event);
    }

    if (r != 0) {
        ELOG << "epoll del ev_read error: " << co::strerror() << ", fd: " << fd;
    }
}

void Epoll::del_ev_write(int fd) {
    auto it = _ev_map.find(fd);
    if (it == _ev_map.end() || !(uint32)it->second) return;

    int r;
    if (!(it->second >> 32)) {
        _ev_map.erase(it);
        r = epoll_ctl(_efd, EPOLL_CTL_DEL, fd, (epoll_event*)8);
    } else {
        it->second &= ((uint64)-1 << 32);
        epoll_event event;
        event.events = EPOLLIN | EPOLLET;
        event.data.u64 = it->second;
        r = epoll_ctl(_efd, EPOLL_CTL_MOD, fd, &event);
    }

    if (r != 0) {
        ELOG << "epoll del ev_write error: " << co::strerror() << ", fd: " << fd;
    }
}

#else  // kqueue
Epoll::Epoll() {
    _kq = kqueue();
    CHECK_NE(_kq, -1) << "kqueue create error: " << co::strerror();

    CHECK_NE(::pipe(_fds), -1) << "create pipe error: " << co::strerror();
    co::set_cloexec(_fds[0]);
    co::set_cloexec(_fds[1]);
    co::set_nonblock(_fds[0]);
    CHECK(this->add_ev_read(_fds[0], 0));
}

Epoll::~Epoll() {
    co::closesocket(_kq);
    co::closesocket(_fds[0]);
    co::closesocket(_fds[1]);
}

bool Epoll::add_ev_read(int fd, void* p) {
    int& x = _ev_map[fd];
    if (x & 1) return true;

    struct kevent event;
    EV_SET(&event, fd, EVFILT_READ, EV_ADD, 0, 0, p);

    if (fp_kevent(_kq, &event, 1, 0, 0, 0) == 0) {
        x |= 1;
        return true;
    }

    ELOG << "kqueue add ev_read error: " << co::strerror() << ", fd: " << fd;
    return false;
}

bool Epoll::add_ev_write(int fd, void* p) {
    int& x = _ev_map[fd];
    if (x & 2) return true;

    struct kevent event;
    EV_SET(&event, fd, EVFILT_WRITE, EV_ADD, 0, 0, p);

    if (fp_kevent(_kq, &event, 1, 0, 0, 0) == 0) {
        x |= 2;
        return true;
    }

    ELOG << "kqueue add ev_write error: " << co::strerror() << ", fd: " << fd;
    return false;
}

void Epoll::del_ev_read(int fd) {
    auto it = _ev_map.find(fd);
    if (it == _ev_map.end() || !(it->second & 1)) return;

    int& x = it->second;
    x == 1 ? (void) _ev_map.erase(it) : (void) (--x);

    struct kevent event;
    EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, 0);

    if (fp_kevent(_kq, &event, 1, 0, 0, 0) != 0) {
        ELOG << "kqueue del ev_read error: " << co::strerror() << ", fd: " << fd;
    }
}

void Epoll::del_ev_write(int fd) {
    auto it = _ev_map.find(fd);
    if (it == _ev_map.end() || !(it->second & 2)) return;

    int& x = it->second;
    x == 2 ? (void) _ev_map.erase(it) : (void) (x -= 2);

    struct kevent event;
    EV_SET(&event, fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);

    if (fp_kevent(_kq, &event, 1, 0, 0, 0) != 0) {
        ELOG << "kqueue del ev_write error: " << co::strerror() << ", fd: " << fd;
    }
}

void Epoll::del_event(int fd) {
    auto it = _ev_map.find(fd);
    if (it == _ev_map.end()) return;

    int ev = it->second;
    _ev_map.erase(it);

    int i = 0;
    struct kevent event[2];
    if (ev & 1) EV_SET(&event[i++], fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
    if (ev & 2) EV_SET(&event[i++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);

    if (fp_kevent(_kq, event, i, 0, 0, 0) != 0) {
        ELOG << "kqueue del error: " << co::strerror() << ", fd: " << fd;
    }
}

#endif
#endif // _WIN32

} // co
