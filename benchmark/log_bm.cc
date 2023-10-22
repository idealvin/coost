#include "co/all.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

DEF_int32(n, 1000000, "amount of logs");
DEF_int32(s, 32, "length of string");
DEF_int32(t, 1, "thread num");

co::sync_event gEv;
fastring gCo;
fastring gSpd;
int gCoCount;
int gSpdCount;

auto logger = spdlog::basic_logger_mt("basic_logger", "logs/spd.log", false);

void co_run(int n) {
    for (int k = 0; k < n; ++k) {
        LOG << gCo.c_str() << 3;
    }
    if (atomic_dec(&gCoCount) == 0) gEv.signal();
}

void spd_run(int n) {
    for (int k = 0; k < n; ++k) {
        logger->info(gSpd.c_str(), 3);
    }
    if (atomic_dec(&gSpdCount) == 0) gEv.signal();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);

    gCo = fastring('x', FLG_s);
    gSpd = fastring('x', FLG_s);
    gSpd.append("{}");
    gCo.c_str();
    gSpd.c_str();

    gCoCount = FLG_t;
    gSpdCount = FLG_t;

    int N = FLG_n / FLG_t;
    int64 write_to_cache;
    int64 write_to_file;

    co::Timer t;
    {
        t.restart();
        for (int i = 0; i < FLG_t; ++i) std::thread(co_run, N).detach();
        gEv.wait();
        write_to_cache = t.us();
	    log::exit();
        write_to_file = t.us();
        cout << "co/log: all logs written to cache in " << write_to_cache << " us\n";
        cout << "co/log: all logs written to file in " << write_to_file << " us\n";
    }

    {
        t.restart();
        for (int i = 0; i < FLG_t; ++i) std::thread(spd_run, N).detach();
        gEv.wait();
        write_to_cache = t.us();
        logger->flush();
        write_to_file = t.us();
        cout << "spdlog: all logs written to cache in " << write_to_cache << " us\n";
        cout << "spdlog: all logs written to file in " << write_to_file << " us\n";
    }

    return 0;
}
