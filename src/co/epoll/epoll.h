#pragma once

#ifdef __linux__
#include "co/sock.h"

namespace co {

struct __cacheline_aligned Epoll {
    Epoll();
    ~Epoll();

    bool add_ev_read(sock_t fd, void* c);
    bool add_ev_write(sock_t fd, void* c);
    void del_ev_read(sock_t fd)  { this->del_event(fd); }
    void del_ev_write(sock_t fd) { this->del_event(fd); }
    void del_event(sock_t fd);

    int wait(int ms);
    void signal();
    void handle_events();
    void _handle_ev_pipe();

    union {
        char _buf[co::cache_line_size];
        uint8 _signaled;
    };
    int _ep;
    int _efd;
    int _n;
    void* _events;
    constexpr static int N = 256;
};

} // co

#endif
