#pragma once

#include <functional>
#include <type_traits>

class Closure {
  public:
    Closure() = default;
    virtual ~Closure() = default;
    
    virtual void run() = 0;
};

namespace xx {

/**
 * function without parameter 
 */
class Function0 : public Closure {
  public:
    typedef void (*F)();

    Function0(F f) : _f(f) {}

    virtual void run() {
        _f();
        delete this;
    }

  private:
    F _f;

    virtual ~Function0() {}
};

/**
 * function with a single parameter 
 *
 * @tparam F  a function type, with only one parameter.
 * @tparam P  parameter of F.
 */
template<typename F, typename P>
class Function1 : public Closure {
  public:
    Function1(F f, P&& p) : _f(f), _p(std::forward<P>(p)) {}

    virtual void run() {
        _f(_p);
        delete this;
    }

  private:
    typename std::remove_reference<F>::type _f;
    typename std::remove_reference<P>::type _p;

    virtual ~Function1() {}
};

/**
 * method (function in a class) without parameter
 * 
 * @tparam T  type of a class.
 */
template<typename T>
class Method0 : public Closure {
  public:
    typedef void (T::*F)();

    Method0(F f, T* o) : _f(f), _o(o) {}

    virtual void run() {
        (_o->*_f)();
        delete this;
    }

  private:
    F _f;
    T* _o;

    virtual ~Method0() {}
};

/**
 * method (function in a class) with a single parameter 
 * 
 * @tparam F  method type, with only one parameter.
 * @tparam T  type of the class. 
 * @tparam P  parameter of F.
 */
template<typename F, typename T, typename P>
class Method1 : public Closure {
  public:
    Method1(F f, T* o, P&& p)
        : _f(f), _o(o), _p(std::forward<P>(p)) {
    }

    virtual void run() {
        (_o->*_f)(_p);
        delete this;
    }

  private:
    typename std::remove_reference<F>::type _f;
    typename std::remove_reference<P>::type _p;
    T* _o;

    virtual ~Method1() {}
};

class Function : public Closure {
  public:
    typedef std::function<void()> F;

    Function(F&& f) : _f(std::move(f)) {}

    Function(const F& f) : _f(f) {}

    virtual void run() {
        _f();
        delete this;
    }

  private:
    F _f;

    virtual ~Function() {}
};

} // xx

inline Closure* new_closure(void (*f)()) {
    return new xx::Function0(f);
}

template<typename F, typename P>
inline Closure* new_closure(F f, P&& p) {
    return new xx::Function1<F, P>(f, std::forward<P>(p));
}

template<typename T>
inline Closure* new_closure(void (T::*f)(), T* o) {
    return new xx::Method0<T>(f, o);
}

template<typename F, typename T, typename P>
inline Closure* new_closure(F f, T* o, P&& p) {
    return new xx::Method1<F, T, P>(f, o, std::forward<P>(p));
}

inline Closure* new_closure(std::function<void()>&& f) {
    return new xx::Function(std::move(f));
}

inline Closure* new_closure(const std::function<void()>& f) {
    return new xx::Function(f);
}
