#include "co/benchmark.h"
#include "co/fastring.h"
#include "co/cout.h"

namespace bm {
namespace xx {

int calc_iters(int64 ns) {
    if (ns <= 1000) return 100 * 1000;
    if (ns <= 10000) return 10 * 1000;
    if (ns <= 100000) return 1000;
    if (ns <= 1000000) return 100;
    if (ns <= 10000000) return 10;
    return 1;
}

static co::vector<Group>* g_g;

inline co::vector<Group>& groups() {
    return g_g ? *g_g : *(g_g = co::_make_static<co::vector<Group>>());
}

bool add_group(const char* name, void (*f)(Group&)) {
    groups().push_back(Group(name, f));
    return true;
}

// do nothing, just fool the compiler
void use(void*, int) {}

struct Num {
    constexpr Num(double v) noexcept : v(v) {}

    fastring str() const {
        fastring s(16);
        if (v < 0.01) {
            s = "< 0.01";
        } else if (v < 1000.0) {
            s << dp::_2(v);
        } else if (v < 1000000.0) {
            s << dp::_2(v / 1000) << 'K';
        } else if (v < 1000000000.0) {
            s << dp::_2(v / 1000000) << 'M';
        } else {
            const double x = v / 1000000000;
            if (x <= 1000.0) {
                s << dp::_2(x) << 'G';
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

    cout << "|  " << text::bold(g.name).blue() << fastring(maxlen - grplen + 2, ' ')
         << "|  " << text::bold("ns/iter  ").blue()
         << "|  " << text::bold("iters/s  ").blue()
         << "|  " << text::bold("speedup  ").blue() << "|\n";

    cout << "| " << fastring(maxlen + 2, '-') << ' '
         << "| " << fastring(9, '-') << ' '
         << "| " << fastring(9, '-') << ' '
         << "| " << fastring(9, '-') << ' ' << "|\n";

    for (size_t i = 0; i < g.res.size(); ++i) {
        auto& r = g.res[i];
        const size_t bmlen = ::strlen(r.bm);
        fastring t = Num(r.ns).str();
        size_t p = t.size() <= 7 ? 9 - t.size() : 2;

        cout << "|  " << text::green(r.bm) << fastring(maxlen - bmlen + 2, ' ')
             << "|  " << text::red(t) << fastring(p, ' ');

        double x = r.ns > 0 ? 1000000000.0 / r.ns : 1.2e12;
        t = Num(x).str();
        p = t.size() <= 7 ? 9 - t.size() : 2;
        cout << "|  " << text::red(t) << fastring(p, ' ');

        if (i == 0) {
            t = "1.0";
        } else {
            auto _ = g.res[0].ns;
            x = r.ns > 0 ? _ / r.ns : (_ > 0 ? 1.2e12 : 1.0);
            t = Num(x).str();
        }

        p = t.size() <= 7 ? 9 - t.size() : 2;
        cout << "|  " << text::yellow(t) << fastring(p, ' ') << "|\n";
    }
}

} // xx

void run_benchmarks() {
    auto& groups = xx::groups();
    for (size_t i = 0; i < groups.size(); ++i) {
        if (i != 0) cout << '\n';
        auto& g = groups[i];
        g.f(g);
        xx::print_results(g);
    }
}

} // bm
