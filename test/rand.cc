#include "co/rand.h"
#include "co/stl.h"
#include "co/print.h"
#include "co/thread.h"
#include "co/time.h"

std::mutex g_mtx;
co::map<uint32, co::vector<co::string>> g_map;
int g_count = 0;

void f() {
    co::vector<co::string> v;
    v.reserve(8);
    v.emplace_back(co::randstr(8));
    v.emplace_back(co::randstr());
    v.emplace_back(co::randstr(32));
    v.emplace_back(co::randstr(64));
    v.emplace_back(co::randstr("0-9", 8));
    v.emplace_back(co::randstr("0-9", 32));
    v.emplace_back(co::randstr("a-z", 64));
    v.emplace_back(co::randstr("0-9a-z", 64));

    std::lock_guard<std::mutex> g(g_mtx);
    g_map.emplace(co::thread_id(), std::move(v));
    ++g_count;
}

int main(int argc, char** argv) {
    for (int i = 0; i < 8; ++i) std::thread(f).detach();
    while (g_count != 8) time::sleep(100);
    for (auto& x : g_map) {
        co::println(x.first, ": ");
        for (auto& v : x.second) {
            co::println("    ", v);
        }
    }
    return 0;
}
