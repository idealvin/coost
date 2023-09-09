#include "co/closure.h"
#include "co/cout.h"

template <class F, typename P>
class X : public co::Closure {
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
        co::print("Y()");
    }

    Y(const Y& y) : _v(y._v) {
        co::print("Y(&)");
    }

    Y(Y&& y) : _v(y._v) {
        y._v = 0;
        co::print("Y(&&)");
    }

    int value() const {
        return _v;
    }

  private:
    int _v;
};

void funca(const Y& v) {
    co::print("funca: hello ", v.value());
}

void funcb(const Y* v) {
    co::print("funcb: hello ", v->value());
}

template <class F, typename T>
inline co::Closure* create_closure(F f, T&& t) {
    return new X<F, T>(f, std::forward<T>(t));
}

void test() {
    Y* s = new Y(777);
    const Y* cs = new Y(888);

    co::Closure* c = create_closure(funca, *s);
    co::Closure* cc = create_closure(funca, *cs);
    co::Closure* rc = create_closure(funca, Y(666));

    co::Closure* bc = create_closure(funcb, s);
    co::Closure* bcc = create_closure(funcb, cs);

    // bc, bcc stores pointer of Y, we must not delete Y before co::Closure::run() is called.
    bc->run();
    bcc->run();
    delete bc;
    delete bcc;

    // c, cc, rc stores a copy of Y, we can delete Y before co::Closure::run() is called. 
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
    co::print("f0()");
}

void f1(int v) {
    co::print("f1: ", v);
}

void test_new_closure() {
    auto f = [](int v) {
        co::print("xxxxx: ", v);
    };

    co::Closure* c = co::new_closure(&f, 3);
    c->run();

    co::Closure* e = co::new_closure(f, 5);
    e->run();

    co::Closure* o0 = co::new_closure(f0);
    o0->run();

    co::Closure* o1 = co::new_closure(f1, 7);
    o1->run();

    co::Closure* o2 = co::new_closure(std::bind(f, 9));
    o2->run();

    co::Closure* o3 = co::new_closure(std::bind(f, std::placeholders::_1), 999);
    o3->run();
}

int main(int argc, char** argv) {
    test();
    test_new_closure();
    return 0;
}
