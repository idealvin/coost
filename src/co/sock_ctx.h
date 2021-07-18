#pragma once

#include "co/def.h"
#include "co/table.h"
#include "co/atomic.h"
#include <string.h>

namespace co {

#ifndef __linux__

class SockCtx {
  public:
    SockCtx() : _x(0) {}

    bool has_event()    const { return _x; }
    bool has_ev_read()  const { return _s.r; }
    bool has_ev_write() const { return _s.w; }

    void add_event() {
        atomic_compare_swap(&_x, 0, (uint16)0x0101);
    }

    void add_ev_read()  { _s.r = 1; }
    void add_ev_write() { _s.w = 1; }
    void del_ev_read()  { _s.r = 0; }
    void del_ev_write() { _s.w = 0; }
    void del_event()    { _x = 0; }

    // On windows, we use _x to save address family of the listening socket.
    void set_address_family(int x) { assert(x < 65536); _x = (uint16)x; }
    int get_address_family() const { return _x; }

  private:
    union {
        struct {
            uint8 r;
            uint8 w;
        } _s;
        uint16 _x;
    };
};

#else

class SockCtx {
  public:
    SockCtx() = delete; //{ memset(this, 0, sizeof(*this)); }

    // store id and scheduler id of the coroutine that performs read operation.
    void add_ev_read(int sched_id, int co_id) {
        _rev.s = sched_id;
        _rev.c = co_id;
    }

    // store id and scheduler id of the coroutine that performs write operation.
    void add_ev_write(int sched_id, int co_id) {
        _wev.s = sched_id;
        _wev.c = co_id;
    }

    void del_event() { memset(this, 0, sizeof(*this)); }
    void del_ev_read()  { _r64 = 0; }
    void del_ev_write() { _w64 = 0; }

    bool has_ev_read()  const { return _rev.c != 0; }
    bool has_ev_write() const { return _wev.c != 0; }

    bool has_ev_read(int sched_id) const {
        return _rev.s == sched_id && _rev.c != 0;
    }

    bool has_ev_write(int sched_id) const {
        return _wev.s == sched_id && _wev.c != 0;
    }

    bool has_event() const {
        return this->has_ev_read() || this->has_ev_write();
    }

    int32 get_ev_read(int sched_id) const {
        return _rev.s == sched_id ? _rev.c : 0;
    }

    int32 get_ev_write(int sched_id) const {
        return _wev.s == sched_id ? _wev.c : 0;
    }

  private:
    struct event_t {
        int32 s; // scheduler id
        int32 c; // coroutine id
    };
    union { event_t _rev; uint64 _r64; };
    union { event_t _wev; uint64 _w64; };
};

#endif

inline SockCtx& get_sock_ctx(size_t sock) {
    static Table<SockCtx> s_sock_ctx_tb(14, 17);
    return s_sock_ctx_tb[sock];
}

} // co
