#include "co/all.h"

DEF_bool(s, false, "use system allocator");
DEF_int32(n, 100000, "n times");
DEF_int32(t, 1, "thread num");
DEF_int32(x, 97, "x");

void test_fun(int id) {
    int N = FLG_n;
    co::vector<void*> v(N);
    fastream s(1024);
    Timer t;
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
        v[i] = co::fixed_alloc(32);
        //v[i] = co::alloc(32);
    }
    us = t.us();
    avg = us * 1000.0 / N - vavg;
    s << "co alloc avg: " << avg << " ns\n";

    t.restart();
    for (int i = N - 1; i >= 0; --i) {
        co::free(v[i], 32);
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "co free avg: " << avg << " ns\n";

    if (FLG_s) {
        v.clear();
        t.restart();
        for (int i = 0; i < N; ++i) {
            v[i] = ::malloc(32);
        }
        us = t.us();
        avg = us * 1000.0 / N - vavg;
        s << "malloc avg: " << avg << " ns\n";

        t.restart();
        for (int i = 0; i < N; ++i) {
            ::free(v[i]);
        }
        us = t.us();
        avg = us * 1000.0 / N;
        s << "::free avg: " << avg << " ns\n";
    }

    COUT << "thread " << id << ": " << s;
    v.reset();
}

void test_string() {
    int N = FLG_n;
    fastream s(1024);
    Timer t;
    int64 us;
    double avg = 0;

    t.restart();
    for (int i = 0; i < N; ++i) {
        fastring x;
        for (int k = 0; k < 64; ++k) {
            x.append(32, (char)FLG_x);
        }
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "fastring " << " avg: " << avg << " ns\n";

    t.restart();
    for (int i = 0; i < N; ++i) {
        std::string x;
        for (int k = 0; k < 64; ++k) {
            x.append(32, (char)FLG_x);
        }
    }
    us = t.us();
    avg = us * 1000.0 / N;
    s << "std::string " << " avg: " << avg << " ns";
    COUT << s;
}

void test_vector() {
    int N = FLG_n;
    fastream s(1024);
    Timer t;
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
    COUT << s;
}

void test_map() {
    int N = FLG_n;
    fastream s(1024);
    Timer t;
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
    COUT << s;
}

void test_unordered_map() {
    int N = FLG_n;
    fastream s(1024);
    Timer t;
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
    COUT << s;
}

int main(int argc, char** argv) {
    flag::init(argc, argv);

    for (int i = 0; i < FLG_t; ++i) {
        Thread(test_fun, i).detach();
    }

    test_string();
    test_vector();
    test_map();
    test_unordered_map();

    while (true) sleep::sec(8);

    return 0;
}
