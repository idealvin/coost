#include "co/os.h"
#include "co/print.h"

int main(int argc, char** argv) {
    co::println("homedir: ", os::homedir());
    co::println("cwd: ", os::cwd());
    co::println("exepath: ", os::exepath());
    co::println("exename: ", os::exename());
    co::println("pid: ", os::pid());
    co::println("cpucores: ", os::cpunum());
    co::println("pagesize: ", os::pagesize());
    return 0;
}
