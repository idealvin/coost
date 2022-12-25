#pragma once

#include "../def.h"
#include "../atomic.h"

namespace co {

/**
 * co::event is for communications between coroutines
 *   - It can be also used in non-coroutines since v2.0.1.
 */
class __coapi event {
  public:
    explicit event(bool manual_reset=false, bool signaled=false);
    ~event();

    event(event&& e) noexcept : _p(e._p) {
        e._p = 0;
    }

    // copy constructor, just increment the reference count
    event(const event& e) : _p(e._p) {
        atomic_inc(_p, mo_relaxed);
    }

    void operator=(const event&) = delete;

    /**
     * wait for a signal
     *   - It blocks until a signal was present.
     *   - It can be called from anywhere since co 2.0.1.
     */
    void wait() const {
        (void) this->wait((uint32)-1);
    }

    /**
     * wait for a signal with a timeout
     *   - It blocks until a signal was present or timeout.
     *   - It can be called from anywhere since co 2.0.1.
     *
     * @param ms  timeout in milliseconds, if ms is -1, never timed out.
     *
     * @return    true if a signal was present before timeout, otherwise false
     */
    bool wait(uint32 ms) const ;

    /**
     * generate a signal on this event
     *   - It can be called from anywhere.
     *   - It will wake up all the waiting coroutines and threads.
     */
    void signal() const;

    // reset the state of the event to unsignaled.
    void reset() const;

  private:
    uint32* _p;
};

typedef event Event;

} // co
