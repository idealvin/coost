#include "co/co.h"
#include "co/print.h"
#include "co/flag.h"

// run main() in coroutine

int _main(int argc, char** argv); 

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    int r;
    co::wait_group wg(1);
    go([&](){
        r = _main(argc, argv);
        wg.done();
    });
    wg.wait();
    return r;
}

int _main(int argc, char** argv) {
    co::println("hello co");
    return 0;
}
