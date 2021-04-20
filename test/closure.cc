#include "co/all.h"

template <class F, typename P>
class X : public Closure {
  public:
    X(F f, P&& p)
        : _f(f), _p(std::forward<P>(p)) {
    }

    virtual ~X() {
    }

    virtual void run() {
        _f(_p);
    }

  private:
    F _f;
    typename std::remove_reference<P>::type _p;
};

class Y {
  public:
    Y(int v) : _v(v) {
        COUT << "Y()";
    }

    Y(const Y& y) : _v(y._v) {
        COUT << "Y(&)";
    }

    Y(Y&& y) : _v(y._v) {
        y._v = 0;
        COUT << "Y(&&)";
    }

    int value() const {
        return _v;
    }

  private:
    int _v;
};

void funca(const Y& v) {
    COUT << "funca: hello " << v.value();
}

void funcb(const Y* v) {
    COUT << "funcb: hello " << v->value();
}

template <class F, typename T>
inline Closure* create_closure(F f, T&& t) {
    return new X<F, T>(f, std::forward<T>(t));
}

void test() {
    Y* s = new Y(777);
    const Y* cs = new Y(888);

    Closure* c = create_closure(funca, *s);
    Closure* cc = create_closure(funca, *cs);
    Closure* rc = create_closure(funca, Y(666));

    Closure* bc = create_closure(funcb, s);
    Closure* bcc = create_closure(funcb, cs);

    // bc, bcc stores pointer of Y, we must not delete Y before Closure::run() is called.
    bc->run();
    bcc->run();
    delete bc;
    delete bcc;

    // c, cc, rc stores a copy of Y, we can delete Y before Closure::run() is called. 
    delete s;
    delete cs;
    c->run();
    cc->run();
    rc->run();
    delete c;
    delete cc;
    delete rc;
}

void f0() {
    COUT << "f0()";
}

void f1(int v) {
    COUT << "f1: " << v;
}

void test_new_closure() {
    auto f = [](int v) {
        COUT << "xxxxx: " << v;
    };

    Closure* c = new_closure(&f, 3);
    c->run();

    Closure* e = new_closure(f, 5);
    e->run();

    Closure* o0 = new_closure(f0);
    o0->run();

    Closure* o1 = new_closure(f1, 7);
    o1->run();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    test();
    test_new_closure();

    return 0;
}
