#pragma once

#include "def.h"
#include "closure.h"
#include "flag.h"
#include "log.h"
#include "stl.h"
#include "./co/thread.h"
#include "./co/sock.h"
#include "./co/event.h"
#include "./co/mutex.h"
#include "./co/pool.h"
#include "./co/chan.h"
#include "./co/io_event.h"
#include "./co/wait_group.h"

namespace co {

/**
 * add a task, which will run as a coroutine 
 *   - It is thread-safe and can be called from anywhere. 
 *   - Closure created by new_closure() will delete itself after Closure::run() 
 *     is done. Users MUST NOT delete it manually.
 *   - Closure is an abstract base class, users are free to implement their own 
 *     subtype of Closure. This may be useful if users do not want a Closure to 
 *     delete itself. See details in co/closure.h.
 * 
 * @param cb  a pointer to a Closure created by new_closure(), or an user-defined Closure.
 */
__coapi void go(Closure* cb);

/**
 * add a task, which will run as a coroutine 
 *   - eg.
 *     go(f);               // void f(); 
 *     go([]() { ... });    // lambda 
 *     go(std::bind(...));  // std::bind 
 * 
 *     std::function<void()> x(std::bind(...)); 
 *     go(x);               // std::function<void()> 
 *     go(&x);              // std::function<void()>* 
 *
 *   - If f is a pointer to std::function<void()>, users MUST ensure that the 
 *     object f points to is valid when Closure::run() is running. 
 * 
 * @param f  any runnable object, as long as we can call f() or (*f)().
 */
template<typename F>
inline void go(F&& f) {
    go(new_closure(std::forward<F>(f)));
}

/**
 * add a task, which will run as a coroutine 
 *   - eg.
 *     go(f, 8);   // void f(int);
 *     go(f, p);   // void f(void*);   void* p;
 *     go(f, o);   // void (T::*f)();  T* o;
 * 
 *     std::function<void(P)> x(std::bind(...));
 *     go(x, p);   // P p;
 *     go(&x, p);  // P p; 
 *
 *   - If f is a pointer to std::function<void(P)>, users MUST ensure that the 
 *     object f points to is valid when Closure::run() is running. 
 *
 * @param f  any runnable object, as long as we can call f(p), (*f)(p) or (p->*f)().
 * @param p  parameter of f, or a pointer to an object of class P if f is a method.
 */
template<typename F, typename P>
inline void go(F&& f, P&& p) {
    go(new_closure(std::forward<F>(f), std::forward<P>(p)));
}

/**
 * add a task, which will run as a coroutine 
 *   - eg.
 *     go(f, o, p);   // void (T::*f)(P);  T* o;  P p;
 
 * @param f  a pointer to a method with a parameter in class T.
 * @param t  a pointer to an object of class T.
 * @param p  parameter of f.
 */
template<typename F, typename T, typename P>
inline void go(F&& f, T* t, P&& p) {
    go(new_closure(std::forward<F>(f), t, std::forward<P>(p)));
}

/**
 * define main function
 *   - DEF_main can be used to ensure code in main function also runs in coroutine. 
 */
#define DEF_main(argc, argv) \
int _co_main(int argc, char** argv); \
int main(int argc, char** argv) { \
    flag::init(argc, argv); \
    int r; \
    co::WaitGroup wg(1); \
    go([&](){ \
        r = _co_main(argc, argv); \
        wg.done(); \
    }); \
    wg.wait(); \
    return r; \
} \
int _co_main(int argc, char** argv)


class __coapi Scheduler {
  public:
    void go(Closure* cb);

    template<typename F>
    inline void go(F&& f) {
        this->go(new_closure(std::forward<F>(f)));
    }

    template<typename F, typename P>
    inline void go(F&& f, P&& p) {
        this->go(new_closure(std::forward<F>(f), std::forward<P>(p)));
    }

    template<typename F, typename T, typename P>
    inline void go(F&& f, T* t, P&& p) {
        this->go(new_closure(std::forward<F>(f), t, std::forward<P>(p)));
    }

  protected:
    Scheduler() = default;
    ~Scheduler() = default;
};

/**
 * get all schedulers 
 *   
 * @return  a reference of an array, which stores pointers to all the Schedulers
 */
__coapi const co::vector<Scheduler*>& schedulers();

/**
 * get the current scheduler
 * 
 * @return a pointer to the current scheduler, or NULL if called from a non-scheduler thread.
 */
__coapi Scheduler* scheduler();

/**
 * get the current coroutine
 * 
 * @return a pointer to the current coroutine
 */
__coapi void* coroutine();

/**
 * get next scheduler 
 *   - It is useful when users want to create coroutines in the same scheduler. 
 *   - eg. 
 *     auto s = co::next_scheduler();
 *     s->go(f);     // void f();
 *     s->go(g, 7);  // void g(int);
 * 
 * @return a non-null pointer.
 */
__coapi Scheduler* next_scheduler();

/**
 * get number of schedulers 
 *   - scheduler id is from 0 to scheduler_num() - 1. 
 *   - This function may be used to implement scheduler-local storage:  
 *                co::vector<T> xx(co::scheduler_num());  
 *     xx[co::scheduler_id()] can be used in a coroutine to access the storage for 
 *     the current scheduler thread.
 * 
 * @return  total number of the schedulers.
 */
__coapi int scheduler_num();

/**
 * get id of the current scheduler 
 *   - It is EXPECTED to be called in a coroutine. 
 * 
 * @return  a non-negative id of the current scheduler, or -1 if the current thread 
 *          is not a scheduler thread.
 */
__coapi int scheduler_id();

/**
 * get id of the current coroutine 
 *   - It is EXPECTED to be called in a coroutine. 
 *   - Each cocoutine has a unique id. 
 * 
 * @return  a non-negative id of the current coroutine, or -1 if the current thread 
 *          is not a scheduler thread.
 */
__coapi int coroutine_id();

/**
 * add a timer for the current coroutine 
 *   - It MUST be called in a coroutine.
 *   - Users MUST call yield() to suspend the coroutine after a timer was added.
 *     When the timer expires, the scheduler will resume the coroutine.
 *
 * @param ms  timeout in milliseconds.
 */
__coapi void add_timer(uint32 ms);

/**
 * add an IO event on a socket to the epoll 
 *   - It MUST be called in a coroutine.
 *   - Users MUST call yield() to suspend the coroutine after an event was added.
 *     When the event is present, the scheduler will resume the coroutine.
 *
 * @param fd  the socket.
 * @param ev  an IO event, either ev_read or ev_write.
 *
 * @return    true on success, false on error.
 */
__coapi bool add_io_event(sock_t fd, io_event_t ev);

/**
 * delete an IO event from epoll
 *   - It MUST be called in a coroutine.
 */
__coapi void del_io_event(sock_t fd, io_event_t ev);

/**
 * remove all IO events on the socket 
 *   - It MUST be called in a coroutine.
 */
__coapi void del_io_event(sock_t fd);

/**
 * suspend the current coroutine 
 *   - It MUST be called in a coroutine. 
 *   - Usually, users should add an IO event, or a timer, or both in a coroutine, 
 *     and then call yield() to suspend the coroutine. When the event is present 
 *     or the timer expires, the scheduler will resume the coroutine. 
 */
__coapi void yield();

/**
 * resume the coroutine
 *   - It is thread safe and can be called anywhere.
 * 
 * @param co  a pointer to the coroutine (result of co::coroutine())
 */
__coapi void resume(void* co);

/**
 * sleep for milliseconds 
 *   - It is EXPECTED to be called in a coroutine. 
 * 
 * @param ms  time in milliseconds
 */
__coapi void sleep(uint32 ms);

/**
 * check whether the current coroutine has timed out 
 *   - It MUST be called in a coroutine.
 *   - When a coroutine returns from an API with a timeout like co::recv, users may 
 *     call co::timeout() to check whether the API call has timed out. 
 * 
 * @return  true if timed out, otherwise false.
 */
__coapi bool timeout();

/**
 * check whether a pointer is on the stack of the current coroutine 
 *   - It MUST be called in a coroutine. 
 */
__coapi bool on_stack(const void* p);

// stop coroutine schedulers
__coapi void stop_schedulers();

} // namespace co

using co::go;
