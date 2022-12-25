#include "co/all.h"
#include "co/cout.h"
#include "co/stref.h"

struct XX {
    XX() {
        cout << "X()\n";
    }
    ~XX() {
        cout << "~X()\n";
    }

    const char* c_str() const { return "xx"; }
    const char* data() const { return "xx"; }
    size_t size() const { return 2; }
};

int main(int argc, char** argv) {
    flag::init(argc, argv);
    COUT << god::has_method_c_str<std::string&>();
    COUT << god::has_method_c_str<std::string>();
    std::string xx("hello");
    co::stref s("hello");
    COUT << s << "|" << co::stref("world") << "| again" << xx;

    fastream fs;
    fs << "hello fs";
    cout << fs << " hello again" << XX() << endl;

    fs.append("hello");

    fs.maxdp(3) << 3.14 << "hello ";
    cout << fs << endl;

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
    cout << text::red(XX()) << '\n';

    return 0;
}
