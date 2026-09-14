#include "co/log.h"
#include "co/print.h"
#include "co/time.h"

DEF_bool(perf, false, "performance testing");

bool static_log() {
    log::debug("hello static");
    return true;
}

bool __ = static_log();

int nested_log() {
    log::debug(">>>> nested log..");
    return 123;
}

int main(int argc, char** argv) {
    flag::parse(argc, argv, true);

    if (FLG_perf) {
        // test performance by writting 100W logs
        co::println("print 100W logs, every log is about 50 bytes");

        time::timer t;
        for (int k = 0; k < 1000000; k++) {
            log::info("hello world ", 3);
        }
        int64 write_to_cache = t.us();

        log::close();
        int64 write_to_file = t.us();

        co::println("All logs written to cache in ", write_to_cache, " us");
        co::println("All logs written to file in ", write_to_file, " us");

    } else {
        // usage of other logs
        log::debug("This is DLOG (debug).. ", 23);
        log::info("This is LOG  (info).. ", 23);
        log::warn("This is WLOG (warning).. ", 23);
        log::error("This is ELOG (error).. ", 23);
        //FLOG << "This is FLOG (fatal).. " << 23;
        log::info("hello ", nested_log(), "  ", nested_log());
    }

    return 0;
}
