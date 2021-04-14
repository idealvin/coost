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
 *     hers own subtype of Closure. See details in co/closure.h.
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
 * @param f  a pointer to a function with a parameter:  void xxx(void*)
 * @param p  a pointer as the parameter of f
 */
inline void go(void (*f)(void*), void* p) {
    go(new_closure(f, p));
}

/**
 * add a task, which will run as a coroutine 
 * 
 * @tparam T  type of a class
 * @param f   a pointer to a method of class T:  void T::xxx()
 * @param o   a pointer to an object of class T
 */
template<typename T>
inline void go(void (T::*f)(), T* o) {
    go(new_closure(f, o));
}

/**
 * add a task, which will run as a coroutine 
 * 
 * @tparam T  type of a class
 * @param f   a pointer to a method of class T:  void T::xxx(void*)
 * @param o   a pointer to an object of class T
 * @param p   a pointer as the parameter of f
 */
template<typename T>
inline void go(void (T::*f)(void*), T* o, void* p) {
    go(new_closure(f, o, p));
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
 * get max number of schedulers 
 *   - scheduler id is from 0 to max_sched_num - 1. 
 * 
 * @return  a positive value, it is equal to the number of CPU cores.
 */
inline int max_sched_num() {
    return xx::max_sched_num();
}

/**
 * get id of the current scheduler 
 *   - It is EXPECTED to be called in a coroutine. 
 * 
 * @return  non-negative id of the current scheduler, or -1 if the current thread 
 *          is not a scheduler thread.
 */
inline int scheduler_id() {
    return scheduler() ? scheduler()->id() : -1;
}

/**
 * get id of the current coroutine 
 *   - Coroutines in different Schedulers may have the same id.
 * 
 * @return  non-negative id of the current coroutine, 
 *          or -1 if it is not called in a coroutine.
 */
inline int coroutine_id() {
    return (scheduler() && scheduler()->running()) ? scheduler()->running()->id : -1;
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
 * stop all coroutine schedulers 
 *   - It is safe to call stop() from anywhere. 
 */
inline void stop() {
    xx::scheduler_manager()->stop_all_schedulers();
}

} // namespace co

using co::go;
