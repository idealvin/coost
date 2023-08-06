#include "co/all.h"
#include "co/mem.h"

DEF_bool(s, false, "use system allocator");
DEF_int32(n, 50000, "n");
DEF_int32(m, 200, "m");
DEF_int32(t, 1, "thread num");
DEF_bool(xfree, false, "test xfree");

void test_fun(int id) {
    int N = FLG_n;
    co::vector<void*> v(N);
    fastream s(1024);
    co::Timer t;
    int64 us;
    double avg;
    double vavg;
    int x;
    int* p = &x;

    for (int i = 0; i < N; ++i) {
        v.push_back((void*)p);
    }

    v.clear();
    t.restart();
    for (int i = 0; i < N; ++i) {
        v[i] = (void*)p;
    }
    us = t.us();
    vavg = us * 1000.0 / N;

    v.clear();
    t.restart();
    for (int i = 0; i < N; ++i) {
        v[i] = co::alloc(32);
    }
    us = t.us();
    avg = us * 1000.0 / N - vavg;
    s << "co::alloc avg: " << avg << " ns\n";

    t.restart();
    for (int i = N - 1; i >= 0; --i) {
        co::free(v[i], 32);
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "co::free avg: " << avg << " ns\n";

    if (FLG_s) {
        v.clear();
        t.restart();
        for (int i = 0; i < N; ++i) {
            v[i] = ::malloc(32);
        }
        us = t.us();
        avg = us * 1000.0 / N - vavg;
        s << "::malloc avg: " << avg << " ns\n";

        t.restart();
        for (int i = 0; i < N; ++i) {
            ::free(v[i]);
        }
        us = t.us();
        avg = us * 1000.0 / N;
        s << "::free avg: " << avg << " ns\n";
    }

    co::print("thread ", id, ":\n", s);
    v.reset();
}

void test_string() {
    int N = FLG_n;
    fastream s(1024);
    co::Timer t;
    int64 us;
    double avg = 0;

    t.restart();
    for (int i = 0; i < N; ++i) {
        fastring x;
        for (int k = 0; k < 64; ++k) {
            x.append(32, 'x');
        }
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "fastring " << " avg: " << avg << " ns\n";

    t.restart();
    for (int i = 0; i < N; ++i) {
        std::string x;
        for (int k = 0; k < 64; ++k) {
            x.append(32, 'x');
        }
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "std::string " << " avg: " << avg << " ns";
    co::print(s);
}

void test_vector() {
    int N = 10000;
    fastream s(1024);
    co::Timer t;
    int64 us;
    double avg = 0;

    co::vector<int> cv;
    std::vector<int> sv;

    t.restart();
    for (int i = 0; i < N; ++i) {
        cv.push_back(i);
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "co::vector " << " avg: " << avg << " ns\n";

    t.restart();
    for (int i = 0; i < N; ++i) {
        sv.push_back(i);
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "std::vector " << " avg: " << avg << " ns";
    co::print(s);
}

void test_map() {
    int N = FLG_n;
    fastream s(1024);
    co::Timer t;
    int64 us;
    double avg = 0;

    co::map<int, int> cm;
    std::map<int, int> sm;

    t.restart();
    for (int i = 0; i < N; ++i) {
        cm.insert(std::make_pair(i, i));
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "co::map " << " avg: " << avg << " ns\n";

    t.restart();
    for (int i = 0; i < N; ++i) {
        sm.insert(std::make_pair(i, i));
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "std::map " << " avg: " << avg << " ns";
    co::print(s);
}

void test_unordered_map() {
    int N = FLG_n;
    fastream s(1024);
    co::Timer t;
    int64 us;
    double avg = 0;

    co::hash_map<int, int> cm;
    std::unordered_map<int, int> sm;

    t.restart();
    for (int i = 0; i < N; ++i) {
        cm.insert(std::make_pair(i, i));
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "co::hash_map " << " avg: " << avg << " ns\n";

    t.restart();
    for (int i = 0; i < N; ++i) {
        sm.insert(std::make_pair(i, i));
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "std::unordered_map " << " avg: " << avg << " ns";
    co::print(s << '\n');
}

auto& gMtx = *co::make_static<std::mutex>();
auto& gA = *co::make_static<co::vector<void*>>(1024 * 1024);
auto& gB = *co::make_static<co::vector<void*>>(1024 * 1024);

void test_xalloc() {
    co::Timer t;
    for (int i = 0; i < FLG_n; ++i) {
        for (int k = 0; k < FLG_m; ++k) {
            void* p = co::alloc(32);
            {
                std::lock_guard<std::mutex> g(gMtx);
                gA.push_back(p);
            }
        }
        if (i % 100 == 0) co::sleep(1);
    }
    co::print("xalloc done in ", t.ms(), "ms");
}

void test_xfree() {
    size_t n = FLG_m * FLG_n;
    co::Timer t;
    while (true) {
        {
            std::lock_guard<std::mutex> g(gMtx);
            gB.swap(gA);
        }
        if (!gB.empty()) {
            for (auto& x : gB) {
                co::free(x, 32);
            }
            n -= gB.size();
            gB.clear();
            if (n == 0) break;
        } else {
            co::sleep(1);
        }
    }
    co::print("xfree done in ", t.ms(), "ms");
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);

    if (!FLG_xfree) {
        test_string();
        test_vector();
        test_map();
        test_unordered_map();

        for (int i = 0; i < FLG_t; ++i) {
            std::thread(test_fun, i).detach();
        }
    } else {
        go(test_xalloc);
        go(test_xfree);
    }

    while (true) sleep::sec(8);

    return 0;
}
