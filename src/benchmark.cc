#include "co/benchmark.h"
#include "co/string.h"
#include "co/print.h"

namespace co {
namespace bm {

static co::vector<Group>* g_g;
static bool g_sub_group_begin;

inline co::vector<Group>& groups() {
    return g_g ? *g_g : *(g_g = []() {
        auto g = co::_make_static<co::vector<Group>>();
        g->reserve(8);
        return g;
    }());
}

bool add_group(const char* name, void (*f)(Group&), bool& e) {
    groups().emplace_back(name, f, e);
    return false;
}

// do nothing, just fool the compiler
void use(void*, int) {}

void sub_group_begin() {
    g_sub_group_begin = true;
}

void Group::start(const char* bm) noexcept {
    c = 1, iters = 0;
    res.emplace_back(bm, 0, res.empty() || g_sub_group_begin);
    if (g_sub_group_begin) g_sub_group_begin = false;
    timer.restart();
}

struct Num {
    constexpr Num(double v) noexcept : v(v) {}

    co::string str() const {
        co::string s(16);
        if (v < 0.01) {
            s = "< 0.01";
        } else if (v < 1000.0) {
            s << co::decimal(v, 2);
        } else if (v < 1000000.0) {
            s << co::decimal(v / 1000, 2) << 'K';
        } else if (v < 1000000000.0) {
            s << co::decimal(v / 1000000, 2) << 'M';
        } else {
            const double x = v / 1000000000;
            if (x <= 1000.0) {
                s << co::decimal(x, 2) << 'G';
            } else {
                s << "> 1000G";
            }
        }
        return s;
    }

    double v;
};

// |  group  |  ns/iter  |  iters/s  |  speedup  |
// | ------- | --------- | --------- | --------- |
// |  bm 0   |  50.0     |  20.0M    |  1.0      |
// |  bm 1   |  10.0     |  100.0M   |  5.0      |
void print_results(Group& g) {
    size_t grplen = ::strlen(g.name);
    size_t maxlen = grplen;
    for (auto& r : g.res) {
        const size_t x = ::strlen(r.bm);
        if (maxlen < x) maxlen = x;
    }

    co::color c;
    const co::string _9_(9, '-');
    co::print(
        "|  ", c.bright_magenta(g.name), co::string(maxlen - grplen + 2, ' '),
        "|  ", c.bright_blue("ns/iter  "),
        "|  ", c.bright_blue("iters/s  "),
        "|  ", c.bright_blue("speedup  "),
        '|', '\n',
        "| ", co::string(maxlen + 2, '-'), ' ',
        "| ", _9_, ' ',
        "| ", _9_, ' ',
        "| ", _9_, ' ',
        '|', '\n'
    );

    double bench_ns = 0;
    for (size_t i = 0; i < g.res.size(); ++i) {
        auto& r = g.res[i];
        const size_t bmlen = ::strlen(r.bm);
        co::string t = Num(r.ns).str();
        size_t p = t.size() <= 7 ? 9 - t.size() : 2;
        if (r.as_bench) bench_ns = r.ns;

        co::print(
            "|  ", c.bright_green(r.bm), co::string(maxlen - bmlen + 2, ' '),
            "|  ", c.bright_cyan(t.c_str()), co::string(p, ' ')
        );

        double x = r.ns > 0 ? 1000000000.0 / r.ns : 1.2e12;
        t = Num(x).str();
        p = t.size() <= 7 ? 9 - t.size() : 2;
        co::print("|  ", c.bright_cyan(t.c_str()), co::string(p, ' '));

        if (r.as_bench) {
            t = "-";
        } else {
            x = r.ns > 0 ? bench_ns / r.ns : (bench_ns > 0 ? 1.2e12 : 1.0);
            t = Num(x).str();
        }

        p = t.size() <= 7 ? 9 - t.size() : 2;
        co::print("|  ", c.bright_yellow(t.c_str()), co::string(p, ' '), '|', '\n').flush();
    }
}

} // bm

void run_benchmarks() {
    auto& groups = bm::groups();
    co::vector<bm::Group*> enabled;
    for (auto& g : groups) {
        if (g.enabled) enabled.push_back(&g);
    }

    if (enabled.empty()) { /* run all benchmarks by default */
        for (size_t i = 0; i < groups.size(); ++i) {
            if (i != 0) co::print('\n');
            auto& g = groups[i];
            g.f(g);
            bm::print_results(g);
        }
    } else {
        for (size_t i = 0; i < enabled.size(); ++i) {
            if (i != 0) co::print('\n');
            auto& g = *enabled[i];
            g.f(g);
            bm::print_results(g);
        }
    }
}

} // co
