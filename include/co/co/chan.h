#pragma once

#include "../def.h"
#include "../thread.h"
#include <deque>

namespace co {

template <typename T>
class Chan {
  public:
    /**
     * @param cap  max capacity of the queue, 1 by default.
     * @param ms   default timeout in milliseconds, -1 by default.
     */
    Chan(size_t cap=1, uint32 ms=(uint32)-1);
    ~Chan();

    bool push(T x) {
        ::MutexGuard g(_mtx);
        if (_dq.size() < _cap) {
            _dq.push_back(x);
            if (_pop > 0) co::xx::cond_notify(&_cond);
            return;
        }

        ++_push;


        bool timedout = false;
        if (_ms == (uint32)-1) {
            co::xx::cond_wait(&_cond, _mtx.mutex());
        } else {
            timedout = co::xx::cond_wait(&_cond, _mtx.mutex(), _ms);
        }
    }

    void operator<<(T x) {
    }

    void operator>>(T& x) {

    }

  private:
    ::Mutex _mtx;
    co::xx::cond_t _cond;
    std::deque<T> _dq;
    size_t _cap;  // max capacity of the queue  
    uint32 _ms;   // default timeout
    uint32 _push; // counter for waiting push
    uint32 _pop;  // counter for waiting pop
    
    DISALLOW_COPY_AND_ASSIGN(Chan);
};

} // co
