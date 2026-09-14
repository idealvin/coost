#pragma once

#if !defined(_WIN32) && !defined(__linux__)
#include "co/sock.h"

namespace co {

struct __cacheline_aligned Kqueue {
    Kqueue();
    ~Kqueue();

    bool add_ev_read(sock_t fd, void* c);
    bool add_ev_write(sock_t fd, void* c);
    void del_ev_read(sock_t fd);
    void del_ev_write(sock_t fd);
    void del_event(sock_t fd);

    int wait(int ms);
    void signal();
    void handle_events();
  
    union {
        char _buf[co::cache_line_size];
        uint8 _signaled;
    };
    int _kq;
    int _n;
    void* _events;
    constexpr static int N = 256;
};

using Epoll = Kqueue;

} // co

#endif
