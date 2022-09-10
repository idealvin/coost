#ifdef __linux__
#pragma once

#include "co/co.h"
#include "co/log.h"
#include "../hook.h"
#include "../sock_ctx.h"
#include <sys/epoll.h>

namespace co {

/**
 * Epoll for Linux 
 *   - We have to consider about that two different coroutines operates on the 
 *     same socket, one for read and one for write. 
 * 
 *     We use data.u64 of epoll_event to store the user data: 
 *       - the higher 32 bits:  id of the coroutine waiting for EV_read. 
 *       - the lower  32 bits:  id of the coroutine waiting for EV_write. 
 * 
 *     When an IO event is present, id in the user data will be used to resume 
 *     the corresponding coroutine.
 */
class Epoll {
  public:
    Epoll(int sched_id);
    ~Epoll();

    bool add_ev_read(int fd, int32 co_id);
    bool add_ev_write(int fd, int32 co_id);
    void del_ev_read(int fd);
    void del_ev_write(int fd);
    void del_event(int fd);

    int wait(int ms) {
        return __sys_api(epoll_wait)(_ep, _ev, 1024, ms);
    }

    // write one byte to the pipe to wake up the epoll.
    void signal(char c = 'x') {
        if (atomic_bool_cas(&_signaled, 0, 1, mo_acq_rel, mo_acquire)) {
            const int r = (int) __sys_api(write)(_pipe_fds[1], &c, 1);
            ELOG_IF(r != 1) << "pipe write error..";
        }
    }

    const epoll_event& operator[](int i)   const { return _ev[i]; }
    int user_data(const epoll_event& ev)         { return ev.data.fd; }
    bool is_ev_pipe(const epoll_event& ev) const { return ev.data.fd == _pipe_fds[0]; }
    void handle_ev_pipe();
    void close();

  private:
    int _ep;
    int _pipe_fds[2];
    int _signaled;
    int _sched_id;
    epoll_event* _ev;
};

} // co

#endif
