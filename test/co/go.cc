#include "co/co.h"
#include "co/flag.h"
#include "co/print.h"
#include <functional>
#include <memory>

void f0() {
    co::println("f0()");
}

void f1(int v) {
    co::println("f1(", v, ")");
}

void f2(const std::string& s) {
    co::println("f2(", s, ")");
}

void f3(void* s) {
    std::unique_ptr<std::string> p((std::string*)s);
    co::println("f3(", *p, ")");
}

void f4(int a, int b) {
    co::println("f4(", a, ',', b, ")");
}

struct T {
    T() = default;
    ~T() = default;

    void m0() {
        co::println("m0()");
    }

    void m1(int v) {
        co::println("m1(", v, ")");
    }

    void m2(const std::string& s) {
        co::println("m2(", s, ")");
    }

    void m3(void* s) {
        std::unique_ptr<std::string> p((std::string*)s);
        co::println("m3(", *p, ")");
    }

    void m4(int a, int b) {
        co::println("m4(", a, ',', b, ")");
    }
};

int main(int argc, char** argv) {
    flag::parse(argc, argv);

    std::string s("s222");
    const std::string cs("cs222");

    go(f0);
    go(f1, 111);

    go(f2, (const char*)"200");
    go(f2, std::string("211"));
    go(f2, s);
    go(f2, cs);

    go(f3, new std::string("333"));
    go(std::bind(f4, 500, 511));

    T o;
    go(&T::m0, &o);
    go(&T::m1, &o, 111);

    go(&T::m2, &o, (const char*)"200");
    go(&T::m2, &o, std::string("211"));
    go(&T::m2, &o, s);
    go(&T::m2, &o, cs);

    go(&T::m3, &o, new std::string("333"));
    go(std::bind(&T::m4, &o, 500, 511));

    go([](int v) {
        co::println("[](", v, ")");
    }, 888);

    go([]() { co::println("[]()"); });

    co::sleep(100);
    return 0;
}
