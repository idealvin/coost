#pragma once

#include "def.h"
#include <mutex>
#include <thread>

namespace co {
namespace xx {

extern __thread uint32 g_tid;
uint32 _thread_id();

} // xx

inline uint32 thread_id() {
    const uint32 x = xx::g_tid;
    return x != 0 ? x : (xx::g_tid = xx::_thread_id());
}

struct sync_event {
    sync_event(bool manual_reset, bool signaled);
    sync_event() : sync_event(false, false) {}
    ~sync_event();

    sync_event(sync_event&& e) noexcept : _p(e._p) { e._p = 0; }

    sync_event(const sync_event&) = delete;
    void operator=(const sync_event&) = delete;
    void operator=(sync_event&&) = delete;

    void notify_one();
    void notify_all();
    void reset();
    void wait();
    bool wait(uint32 ms);

    void* _p;
};

} // co
