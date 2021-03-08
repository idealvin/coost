#pragma once

#include <functional>

class Closure {
  public:
    Closure() = default;
    
    Closure(const Closure&) = delete;
    void operator=(const Closure&) = delete;

    virtual void run() = 0;

  protected:
    virtual ~Closure() = default;
};

namespace xx {

class Function0 : public Closure {
  public:
    typedef void (*F)();

    Function0(F f)
        : _f(f) {
    }

    virtual void run() {
        _f();
        delete this;
    }

  private:
    F _f;

    virtual ~Function0() {}
};

class Function1 : public Closure {
  public:
    typedef void (*F)(void*);

    Function1(F f, void* p)
        : _f(f), _p(p) {
    }

    virtual void run() {
        _f(_p);
        delete this;
    }

  private:
    F _f;
    void* _p;

    virtual ~Function1() {}
};

template<typename T>
class Method0 : public Closure {
  public:
    typedef void (T::*F)();

    Method0(F f, T* p)
        : _f(f), _p(p) {
    }

    virtual void run() {
        (_p->*_f)();
        delete this;
    }

  private:
    F _f;
    T* _p;

    virtual ~Method0() {}
};

class Function : public Closure {
  public:
    typedef std::function<void()> F;

    Function(F&& f)
        : _f(std::move(f)) {
    }

    Function(const F& f)
        : _f(f) {
    }

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

inline Closure* new_closure(void (*f)(void*), void* p) {
    return new xx::Function1(f, p);
}

template<typename T>
inline Closure* new_closure(void (T::*f)(), T* p) {
    return new xx::Method0<T>(f, p);
}

inline Closure* new_closure(std::function<void()>&& f) {
    return new xx::Function(std::move(f));
}

inline Closure* new_closure(const std::function<void()>& f) {
    return new xx::Function(f);
}
