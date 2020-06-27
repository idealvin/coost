#include "test.h"
#include "co/all.h"

fastring fa(int v) {
    return str::from(v);
}

fastring fb(const fastring& s) {
    if (!s.empty() && s.front() != 's') {
        fastring x(s);
        x.front() = 's';
        return x;
    } 
    return s;
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    COUT << "hello world";

    return 0;
}
