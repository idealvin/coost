#include "co/all.h"
#include "co/cout.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);

    fastream fs;
    fs.append("hello");
    fs << dp::_1(3.14) << "hello ";

    cout << text::red("hello\n");
    cout << text::green("hello\n");
    cout << text::blue("hello\n");
    cout << text::yellow("hello\n");
    cout << text::magenta("hello\n");
    cout << text::cyan("hello\n");
    cout << text::bold("hello\n");
    cout << text::bold("hello\n").red();
    cout << text::bold("hello\n").green();
    cout << text::bold("hello\n").blue();
    cout << text::bold("hello\n").yellow();
    cout << text::bold("hello\n").magenta();
    cout << text::bold("hello\n").cyan();
    cout << text::red(fastring(8, 'c')) << '\n';

    return 0;
}
