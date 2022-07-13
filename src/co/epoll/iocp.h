#ifdef _WIN32
#pragma once

#include "co/co.h"
#include "co/log.h"
#include "../hook.h"
#include "../sock_ctx.h"

namespace co {

class Iocp {
  public:
    Iocp(int sched_id);
    ~Iocp();

    bool add_event(sock_t fd) {
        if (fd == (sock_t)-1) return false;
        auto& ctx = co::get_sock_ctx(fd);
        if (ctx.has_event()) return true; // already exists

        if (CreateIoCompletionPort((HANDLE)fd, _iocp, fd, 0) != 0) {
            ctx.add_event();
            return true;
        } else {
            //ELOG << "iocp add socket " << fd << " error: " << co::strerror();
            // always return true here.
            return true;
        }
    }

    // for close
    void del_event(sock_t fd) {
        if (fd != (sock_t)-1) co::get_sock_ctx(fd).del_event();
    }

    // for half-shutdown read
    void del_ev_read(sock_t fd) {
        if (fd != (sock_t)-1) co::get_sock_ctx(fd).del_ev_read();
    }

    // for half-shutdown write
    void del_ev_write(sock_t fd) {
        if (fd != (sock_t)-1) co::get_sock_ctx(fd).del_ev_write();
    }

    int wait(int ms) {
        ULONG n = 0;
        const BOOL r = __sys_api(GetQueuedCompletionStatusEx)(_iocp, _ev, 1024, &n, ms, false);
        if (r == TRUE) return (int)n;
        return ::GetLastError() == WAIT_TIMEOUT ? 0 : -1;
    }

    void signal() {
        if (atomic_bool_cas(&_signaled, 0, 1, mo_acquire, mo_acquire)) {
            const BOOL r = PostQueuedCompletionStatus(_iocp, 0, 0, 0);
            ELOG_IF(!r) << "PostQueuedCompletionStatus error: " << co::strerror();
        }
    }

    const OVERLAPPED_ENTRY& operator[](int i) const { return _ev[i]; }
    void* user_data(const OVERLAPPED_ENTRY& ev) { return ev.lpOverlapped; }
    bool is_ev_pipe(const OVERLAPPED_ENTRY& ev) { return ev.lpOverlapped == 0; }
    void handle_ev_pipe() { atomic_store(&_signaled, 0, mo_release); }

  private:
    HANDLE _iocp;
    OVERLAPPED_ENTRY* _ev;
    int _signaled;
    int _sched_id;
};

typedef OVERLAPPED_ENTRY epoll_event;
typedef Iocp Epoll;

} // co

#endif
