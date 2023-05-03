#pragma once

#include "../def.h"

namespace co {

// It is for communications between coroutines and/or threads.
// Support for threads(non-coroutine) is available since v2.0.1.
class __coapi event {
  public:
    explicit event(bool manual_reset=false, bool signaled=false);
    ~event();

    event(event&& e) noexcept : _p(e._p) {
        e._p = 0;
    }

    // copy constructor, just increment the reference count
    event(const event& e);

    void operator=(const event&) = delete;

    // wait until the event is signaled
    void wait() const {
        (void) this->wait((uint32)-1);
    }

    // Wait until the event is signaled or timed out.
    // Return true if it is signaled before timed out, otherwise false.
    bool wait(uint32 ms) const ;

    // Wake up all waiters. If no waiter is present, mark the event as signaled, 
    // and the next waiter that calls wait() will return immediately.
    void signal() const;

    // reset the event to unsignaled
    void reset() const;

  private:
    void* _p;
};

typedef event Event;

} // co
