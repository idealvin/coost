#include "co/log.h"
#include "co/cout.h"
#include "co/time.h"

DEF_bool(perf, false, "performance testing");

bool static_log() {
    DLOG << "hello static";
    LOG << "hello again, static";
    return true;
}

bool __ = static_log();

int nested_log() {
    DLOG << ">>>> nested log..";
    return 123;
}

int main(int argc, char** argv) {
    flag::init(argc, argv);

    if (FLG_perf) {
        // test performance by writting 100W logs
        COUT << "print 100W logs, every log is about 50 bytes";

        Timer t;
        for (int k = 0; k < 1000000; k++) {
            LOG << "hello world " << 3;
        }
        int64 write_to_cache = t.us();

        log::exit();
        int64 write_to_file = t.us();

        COUT << "All logs written to cache in " << write_to_cache << " us";
        COUT << "All logs written to file in " << write_to_file << " us";

    } else {
        // usage of other logs
        DLOG << "This is DLOG (debug).. " << 23;
        LOG  << "This is LOG  (info).. " << 23;
        WLOG << "This is WLOG (warning).. " << 23;
        ELOG << "This is ELOG (error).. " << 23;
        //FLOG << "This is FLOG (fatal).. " << 23;
        LOG << "hello " << nested_log() << "  " << nested_log();
        TLOG("co") << "hello co";
        TLOG("bob") << "hello bob";
    }

    return 0;
}
