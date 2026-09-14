#include "co/all.h"

DEF_bool(s, false, "use system allocator");
DEF_int32(n, 50000, "n");
DEF_int32(m, 200, "m");
DEF_int32(t, 1, "thread num");
DEF_bool(xfree, false, "test xfree");

co::wait_group wg;

void test_fun(int id) {
    int N = FLG_n;
    co::vector<void*> v;
    v.reserve(N);

    co::string s(1024);
    time::timer t;
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

    co::println("thread ", id, ":\n", s);
    wg.done();
}

void test_string() {
    int N = FLG_n;
    co::string s(1024);
    time::timer t;
    int64 us;
    double avg = 0;

    t.restart();
    for (int i = 0; i < N; ++i) {
        co::string x;
        for (int k = 0; k < 64; ++k) {
            x.append(32, 'x');
        }
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "co::string " << " avg: " << avg << " ns\n";

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
    co::println(s);
}

void test_vector() {
    int N = 10000;
    co::string s(1024);
    time::timer t;
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
    co::println(s);
}

void test_map() {
    int N = FLG_n;
    co::string s(1024);
    time::timer t;
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
    co::println(s);
}

void test_unordered_map() {
    int N = FLG_n;
    co::string s(1024);
    time::timer t;
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
    s << "std::unordered_map " << " avg: " << avg << " ns" << '\n';
    co::println(s);
}

auto& gA = *co::make_static<co::vector<void*>>();

void test_xalloc() {
    gA.reserve(50 * 1024);
    time::timer t;
    for (int i = 0; i < FLG_n; ++i) {
        gA.push_back(co::alloc(32));
    }
    auto us = t.us();
    co::println(co::sched_id(), " xalloc done in ", us, "us, avg: ", us * 1000.0 / FLG_n, "ns");
    wg.done();
}

void test_xfree() {
    time::timer t;
    for (auto& x : gA) { co::free(x, 32); }
    auto us = t.us();
    co::println(co::sched_id(), " xfree done in ", us, "us, avg: ", us * 1000.0 / FLG_n, "ns");
    wg.done();
}

int main(int argc, char** argv) {
    flag::parse(argc, argv, true);

    if (!FLG_xfree) {
        test_string();
        test_vector();
        test_map();
        test_unordered_map();

        wg.add(FLG_t);
        for (int i = 0; i < FLG_t; ++i) {
            std::thread(test_fun, i).detach();
        }
        wg.wait();
    } else {
        wg.add(1);
        go(test_xalloc);
        wg.wait();

        wg.add(1);
        go(test_xfree);
        wg.wait();
    }

    return 0;
}
