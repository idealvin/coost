#include "co/print.h"

int main(int argc, char** argv) {
    co::color c;
    co::print(c.red("red\n"));
    co::print(c.green("green\n"));
    co::print(c.yellow("yellow\n"));
    co::print(c.blue("blue\n"));
    co::print(c.magenta("magenta\n"));
    co::print(c.cyan("cyan\n"));
    co::print(c.bold("bold\n"));
    co::print(c.bright_red("bright_red\n"));
    co::print(c.bright_green("bright_green\n"));
    co::print(c.bright_yellow("bright_yellow\n"));
    co::print(c.bright_blue("bright_blue\n"));
    co::print(c.bright_magenta("bright_magenta\n"));
    co::print(c.bright_cyan("bright_cyan\n"));

    co::print("hello ", c.red, "coost ", 23, '\n', c.deflt).flush();
    co::println("hello ", c.red, "coost ", c.green, 123, c.deflt);
    return 0;
}
