#include "co/cout.h"

int main(int argc, char** argv) {
    cout << text::red("hello\n");
    cout << text::green("hello\n");
    cout << text::blue("hello\n");
    cout << text::yellow("hello\n");
    cout << text::magenta("hello\n");
    cout << text::cyan("hello\n");
    cout << "hello\n";
    cout << text::bold("hello\n");
    cout << text::bold("hello\n").red();
    cout << text::bold("hello\n").green();
    cout << text::bold("hello\n").blue();
    cout << text::bold("hello\n").yellow();
    cout << text::bold("hello\n").magenta();
    cout << text::bold("hello\n").cyan();
    co::print("hello", text::red(" coost "), 23);
    co::print("hello", text::bold(" coost ").green(), 23);
    co::print("hello", text::blue(" coost "), 23);
    return 0;
}
