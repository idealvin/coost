#pragma once

#include "def.h"
#include "mem.h"

#include <functional>
#include <type_traits>

namespace co {

class __coapi Closure {
  public:
    Closure() = default;
    virtual ~Closure() = default;
    
    virtual void run() = 0;
};

namespace xx {

template<typename F>
class Function0 : public Closure {
  public:
    Function0(F&& f) : _f(std::forward<F>(f)) {}
    virtual ~Function0() = default;

    virtual void run() {
        _f();
        co::del(this);
    }

  private:
    typename std::remove_reference<F>::type _f;
};

template<typename F>
class Function0p : public Closure {
  public:
    Function0p(F* f) : _f(f) {}
    virtual ~Function0p() = default;

    virtual void run() {
        (*_f)();
        co::del(this);
    }

  private:
    typename std::remove_reference<F>::type* _f;
};

template<typename F, typename P>
class Function1 : public Closure {
  public:
    Function1(F&& f, P&& p) : _f(std::forward<F>(f)), _p(std::forward<P>(p)) {}
    virtual ~Function1() = default;

    virtual void run() {
        _f(_p);
        co::del(this);
    }

  private:
    typename std::remove_reference<F>::type _f;
    typename std::remove_reference<P>::type _p;
};

template<typename F, typename P>
class Function1p : public Closure {
  public:
    Function1p(F* f, P&& p) : _f(f), _p(std::forward<P>(p)) {}
    virtual ~Function1p() = default;

    virtual void run() {
        (*_f)(_p);
        co::del(this);
    }

  private:
    typename std::remove_reference<F>::type* _f;
    typename std::remove_reference<P>::type _p;
};

template<typename T>
class Method0 : public Closure {
  public:
    typedef void (T::*F)();

    Method0(F f, T* o) : _f(f), _o(o) {}
    virtual ~Method0() = default;

    virtual void run() {
        (_o->*_f)();
        co::del(this);
    }

  private:
    F _f;
    T* _o;
};

template<typename F, typename T, typename P>
class Method1 : public Closure {
  public:
    Method1(F&& f, T* o, P&& p)
        : _f(std::forward<F>(f)), _o(o), _p(std::forward<P>(p)) {
    }
    
    virtual ~Method1() = default;

    virtual void run() {
        (_o->*_f)(_p);
        co::del(this);
    }

  private:
    typename std::remove_reference<F>::type _f;
    T* _o;
    typename std::remove_reference<P>::type _p;
};

} // xx

/**
 * @param f  any runnable object, as long as we can call f().
 */
template<typename F>
inline Closure* new_closure(F&& f) {
    return co::make<xx::Function0<F>>(std::forward<F>(f));
}

/**
 * @param f  pointer to any runnable object, as long as we can call (*f)().
 */
template<typename F>
inline Closure* new_closure(F* f) {
    return co::make<xx::Function0p<F>>(f);
}

/**
 * function with a single parameter 
 *
 * @param f  any runnable object, as long as we can call f(p).
 * @param p  parameter of f.
 */
template<typename F, typename P>
inline Closure* new_closure(F&& f, P&& p) {
    return co::make<xx::Function1<F, P>>(std::forward<F>(f), std::forward<P>(p));
}

/**
 * function with a single parameter 
 *
 * @param f  any runnable object, as long as we can call (*f)(p).
 * @param p  parameter.
 */
template<typename F, typename P>
inline Closure* new_closure(F* f, P&& p) {
    return co::make<xx::Function1p<F, P>>(f, std::forward<P>(p));
}

/**
 * method (function in a class) without parameter
 * 
 * @param f  pointer to a method without parameter in class T.
 * @param o  pointer to an object of T.
 */
template<typename T>
inline Closure* new_closure(void (T::*f)(), T* o) {
    return co::make<xx::Method0<T>>(f, o);
}

/**
 * method (function in a class) with a single parameter 
 * 
 * @tparam F  method type, void (T::*)(P).
 * @tparam T  type of the class.
 * @tparam P  type of the parameter.
 * @param f   pointer to a method with a parameter in class T.
 * @param o   pointer to an object of T.
 * @param p   parameter of f.
 */
template<typename F, typename T, typename P>
inline Closure* new_closure(F&& f, T* o, P&& p) {
    return co::make<xx::Method1<F, T, P>>(std::forward<F>(f), o, std::forward<P>(p));
}

} // co
