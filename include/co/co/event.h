#pragma once

namespace co {

/**
 * co::Event is for communications between coroutines 
 *   - It is similar to SyncEvent for threads. 
 *   - The user SHOULD use co::Event in coroutine environments only. 
 */
class Event {
  public:
    Event();
    ~Event();

    Event(Event&& e) : _p(e._p) { e._p = 0; }

    Event(const Event&) = delete;
    void operator=(const Event&) = delete;

    /**
     * wait for a signal 
     *   - It MUST be called in a coroutine. 
     *   - wait() blocks until a signal was present. 
     */
    void wait();

    /**
     * wait for a signal with a timeout 
     *   - It MUST be called in a coroutine. 
     *   - It blocks until a signal was present or timeout. 
     * 
     * @param ms  timeout in milliseconds
     * 
     * @return    true if a signal was present before timeout, otherwise false
     */
    bool wait(unsigned int ms);

    /**
     * generate a signal on this event 
     *   - It is not necessary to call signal() in a coroutine, though usually it is. 
     *   - When a signal was present, all the waiting coroutines will be waken up. 
     */
    void signal();

  private:
    void* _p;
};

} // co
