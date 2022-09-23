#pragma once

#include "../def.h"
#include "../atomic.h"

namespace co {

/**
 * co::Event is for communications between coroutines
 *   - It is similar to SyncEvent for threads.
 *   - It can be used anywhere since co 2.0.1.
 */
class __coapi Event {
  public:
    explicit Event(bool manual_reset=false, bool signaled=false);
    ~Event();

    Event(Event&& e) : _p(e._p) {
        e._p = 0;
    }

    // copy constructor, allow co::Event to be captured by value in lambda.
    Event(const Event& e) : _p(e._p) {
        atomic_inc(_p, mo_relaxed);
    }

    void operator=(const Event&) = delete;

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
     *   - When a signal was present, all the waiting coroutines will be waken up.
     */
    void signal() const;

    // reset the state of the event to unsignaled.
    void reset() const;

  private:
    uint32* _p;
};

} // co
