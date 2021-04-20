#pragma once

#include "co/sock.h"
#include "co/hook.h"
#include "co/epoll.h"
#include "co/scheduler.h"
#include "co/event.h"
#include "co/mutex.h"
#include "co/pool.h"
#include "co/io_event.h"

namespace co {

/**
 * add a task, which will run as a coroutine 
 *   - If a Closure was created with new_closure(), the user needn't delete it 
 *     manually, as it will delete itself after Closure::run() is done.
 *   - Closure is an abstract base class, the user is free to implement his or 
 *     hers own subtype of Closure. This may be useful if the user do not want 
 *     a Closure to delete itself. See details in co/closure.h.
 * 
 * @param cb  a pointer to a Closure created by new_closure(), or an user-defined Closure.
 */
inline void go(Closure* cb) {
    xx::scheduler_manager()->next_scheduler()->add_new_task(cb);
}

/**
 * add a task, which will run as a coroutine 
 * 
 * @param f  a pointer to a function:  void xxx()
 */
inline void go(void (*f)()) {
    go(new_closure(f));
}

/**
 * add a task, which will run as a coroutine 
 * 
 * @tparam F  function type
 * @tparam P  parameter type
 * @param f   a pointer to a function with a parameter
 * @param p   parameter of f
 */
template<typename F, typename P>
inline void go(F f, P&& p) {
    go(new_closure(f, std::forward<P>(p)));
}

/**
 * add a task, which will run as a coroutine 
 * 
 * @tparam T  type of a class
 * @param f   a pointer to a method of class T, without parameter
 * @param o   a pointer to an object of class T
 */
template<typename T>
inline void go(void (T::*f)(), T* o) {
    go(new_closure(f, o));
}

/**
 * add a task, which will run as a coroutine 
 * 
 * @tparam F  method in a class
 * @tparam T  type of the class
 * @tparam P  parameter of F
 * @param f   a pointer to a method of class T, with one parameter
 * @param o   a pointer to an object of class T
 * @param p   parameter of f
 */
template<typename F, typename T, typename P>
inline void go(F f, T* o, P&& p) {
    go(new_closure(f, o, std::forward<P>(p)));
}

/**
 * add a task, which will run as a coroutine 
 *   - It is a little expensive to create an object of std::function, try to 
 *     avoid it if the user cares about the performance. 
 * 
 * @param f  a reference of an object of std::function<void()>
 */
inline void go(const std::function<void()>& f) {
    go(new_closure(f));
}

/**
 * add a task, which will run as a coroutine 
 *   - It is a little expensive to create an object of std::function, try to 
 *     avoid it if the user cares about the performance. 
 * 
 * @param f  a rvalue reference of an object of std::function<void()>
 */
inline void go(std::function<void()>&& f) {
    go(new_closure(std::move(f)));
}

/**
 * get the current scheduler 
 *   
 * @return  a pointer to the current scheduler, or NULL if the current thread 
 *          is not a scheduler thread.
 */
inline xx::Scheduler* scheduler() {
    return xx::gSched;
}

/**
 * get all schedulers 
 *   
 * @return  a reference of an array, which stores pointers to all the Schedulers
 */
inline const std::vector<xx::Scheduler*>& all_schedulers() {
    return xx::scheduler_manager()->all_schedulers();
}

/**
 * get number of schedulers 
 *   - scheduler id is from 0 to scheduler_num() - 1. 
 *   - This function may be used to implement scheduler-local storage:  
 *                std::vector<T> xx(co::scheduler_num());  
 *     xx[co::scheduler_id()] can be used in a coroutine to access the storage for 
 *     the current scheduler thread.
 * 
 * @return  a positive value
 */
inline int scheduler_num() {
    static int kSchedNum = xx::scheduler_num();
    return kSchedNum;
}

/**
 * get id of the current scheduler 
 *   - It is EXPECTED to be called in a coroutine. 
 * 
 * @return  a non-negative id of the current scheduler, or -1 if the current thread 
 *          is not a scheduler thread.
 */
inline int scheduler_id() {
    return scheduler() ? scheduler()->id() : -1;
}

/**
 * get id of the current coroutine 
 *   - It is EXPECTED to be called in a coroutine. 
 *   - Each cocoutine has a unique id. 
 * 
 * @return  a non-negative id of the current coroutine, or -1 if the current thread 
 *          is not a scheduler thread.
 */
inline int coroutine_id() {
    return scheduler() ? scheduler()->coroutine_id() : -1;
}

/**
 * sleep for milliseconds 
 *   - It is EXPECTED to be called in a coroutine. 
 * 
 * @param ms  time in milliseconds
 */
inline void sleep(unsigned int ms) {
    scheduler() ? scheduler()->sleep(ms) : sleep::ms(ms);
}

/**
 * check whether the current coroutine has timed out 
 *   - When a coroutine returns from an API with a timeout like co::recv, it may 
 *     call co::timeout() to check whether the previous API call has timed out. 
 * 
 * @return  true if timed out, otherwise false.
 */
inline bool timeout() {
    return scheduler() && scheduler()->timeout();
}

/**
 * stop all coroutine schedulers 
 *   - It is safe to call stop() from anywhere. 
 */
inline void stop() {
    xx::scheduler_manager()->stop_all_schedulers();
}

} // namespace co

using co::go;
