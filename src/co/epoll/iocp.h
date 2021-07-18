#pragma once

#include "co/co.h"
#include "co/log.h"
#include "../sock_ctx.h"

namespace co {

class Iocp {
  public:
    Iocp(int thread_num);
    ~Iocp();

    bool add_event(sock_t fd) {
        auto& ctx = co::get_sock_ctx(fd);
        if (ctx.has_event()) return true; // already exists

        if (CreateIoCompletionPort((HANDLE)fd, _iocp, fd, 0) != 0) {
            ctx.add_event();
            return true;
        } else {
            ELOG << "iocp add socket " << fd << " error: " << co::strerror();
            return false;
        }
    }

  private:
    void loop();

  private:
    HANDLE _iocp;
    OVERLAPPED_ENTRY* _ev;
    int _stop;
};

typedef OVERLAPPED_ENTRY epoll_event;

class Epoll {
  public:
    Epoll(uint32 thread_num);
    ~Epoll();

    bool add_event(sock_t fd) {
        if (fd == (sock_t)-1) return false;
        return _iocp->add_event(fd);
    }

    void del_event(sock_t fd) {
        if (fd != (sock_t)-1) co::get_sock_ctx(fd).del_event();
    }

    void del_ev_read(sock_t fd) {
        if (fd != (sock_t)-1) co::get_sock_ctx(fd).del_ev_read();
    }

    void del_ev_write(sock_t fd) {
        if (fd != (sock_t)-1) co::get_sock_ctx(fd).del_ev_write();
    }

    int wait(uint32 ms) {
        _ev.wait(ms);
        if (_signaled) _signaled = false;
        return 0;
    }

    void signal() {
        if (atomic_compare_swap(&_signaled, false, true) == false) _ev.signal();
    }

  private:
    Iocp* _iocp;
    SyncEvent _ev;
    bool _signaled;
};

} // co
