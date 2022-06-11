#include "co/all.h"

co::maybe<void> fvoid(int v) {
    if (v > 0) return 0;
    return co::error("v <= 0");
}

co::maybe<int> fint(int v) {
    if (v > 0) return v;
    return co::error("v <= 0");
}

int gV = 0;

co::maybe<int&> fintref(int v) {
    if (v > 0) return gV;
    return co::error("v <= 0");
}

co::maybe<fastring> ffastring(int v) {
    if (v > 0) return fastring("nice");
    return co::error("v <= 0");
}

int main(int argc, char** argv) {
    flag::init(argc, argv);

    {
        auto a = fvoid(0);
        auto b = fvoid(1);
        COUT << "fvoid";
        COUT << "a.has_error: " << a.has_error();
        COUT << "b.has_error: " << b.has_error();
    }

    {
        auto a = fint(0);
        auto b = fint(1);
        COUT << "fint";
        COUT << "a.has_error: " << a.has_error() << " value: " << a.value();
        COUT << "b.has_error: " << b.has_error() << " value: " << b.value();
    }

    {
        auto a = fintref(0);
        auto b = fintref(1);
        COUT << "fintref";
        COUT << "a.has_error: " << a.has_error();
        COUT << "b.has_error: " << b.has_error();
        COUT << "gV: " << gV;
        b.value() = 3;
        COUT << "gV: " << gV;
    }

    {
        auto a = ffastring(0);
        auto b = ffastring(1);
        COUT << "ffastring";
        COUT << "a.has_error: " << a.has_error();
        COUT << "b.has_error: " << b.has_error();
        COUT << "b.value: " << b.value();
        b.value() = "hello";
        COUT << "b.value: " << b.value();
    }

    return 0;
}
