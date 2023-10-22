#pragma once

#include "co/def.h"
#include "co/mem.h"
#include "co/atomic.h"
#include "co/table.h"

namespace co {

#if defined(_WIN32)

class SockCtx {
  public:
    SockCtx() = delete;

    bool has_event() const { return _x; }

    int add_event() {
        return atomic_cas(&_x, 0, 0x0101, mo_acq_rel, mo_acquire);
    }

    void del_ev_read()  { _s.r = 0; }
    void del_ev_write() { _s.w = 0; }
    void del_event()    { _x = 0; }

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

#elif defined(__linux__)

class SockCtx {
  public:
    SockCtx() = delete;

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

    void del_event() { _r64 = 0; _w64 = 0; }
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
    struct S {
        int32 s; // scheduler id
        int32 c; // coroutine id
    };
    union { S _rev; uint64 _r64; };
    union { S _wev; uint64 _w64; };
};

#else

class SockCtx {
  public:
    SockCtx() = delete;

    bool has_event()    const { return _x; }
    bool has_ev_read()  const { return _s.r; }
    bool has_ev_write() const { return _s.w; }

    void add_event() {
        atomic_cas(&_x, 0, 0x0101, mo_acq_rel, mo_acquire);
    }

    void add_ev_read()  { _s.r = 1; }
    void add_ev_write() { _s.w = 1; }
    void del_ev_read()  { _s.r = 0; }
    void del_ev_write() { _s.w = 0; }
    void del_event()    { _x = 0; }

  private:
    union {
        struct {
            uint8 r;
            uint8 w;
        } _s;
        uint16 _x;
    };
};

#endif

extern co::table<SockCtx>* g_ctx_tb;

inline SockCtx& get_sock_ctx(size_t sock) {
    return (*g_ctx_tb)[sock];
}

} // co
