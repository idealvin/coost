#include "co/def.h"
#include "co/cout.h"
#include "co/flag.h"
#include "co/time.h"

DEF_uint64(beg, 0, "beg");
DEF_uint64(end, 0, "end");

int main(int argc, char** argv) {
    flag::init(argc, argv);

    if (FLG_end == 0) {
        FLG_end = FLG_beg == 0 ? 9999 : FLG_beg + 100000;
    }

    int r = 0;
    char buf[32];
    double svg = 1.0; // avg of snprintf
    double avg = 1.0;

    Timer t;
    int64 us = 0;

    do {
        COUT << "\n======= u64toa vs snprintf =======";
        t.restart();
        for (unsigned long long i = FLG_beg; i < FLG_end; i++) {
            snprintf(buf, 32, "%llu", i);
        }
        us = t.us();
        svg = static_cast<double>(us*1000) / (FLG_end - FLG_beg);
        COUT << "snprintf " << FLG_beg << '~' << FLG_end << ", done in " << us << " us, avg: " << svg << " ns";

        t.restart();
        for (unsigned long long i = FLG_beg; i < FLG_end; i++) {
            fast::u64toa(i, buf);
        }
        us = t.us();
        avg = static_cast<double>(us*1000) / (FLG_end - FLG_beg);
        COUT << "u64toa " << FLG_beg << '~' << FLG_end << ", done in " << us << " us, avg: " << avg << " ns";
        COUT << "-> speedup: " << (svg/avg);
    } while (0);

    do {
        COUT << "\n======= u32toa vs snprintf =======";
        if (FLG_beg <= MAX_UINT32 - 100000) {
            uint32 u32beg = (uint32) FLG_beg;
            uint32 u32end = (uint32) FLG_end;

            t.restart();
            for (uint32 i = u32beg; i < u32end; i++) {
                fast::u32toa(i, buf);
            }
            us = t.us();
            avg = static_cast<double>(us*1000) / (FLG_end - FLG_beg);
            COUT << "u32toa " << FLG_beg << '~' << FLG_end << ", done in " << us << " us, avg: " << avg << " ns";
            COUT << "-> speedup: " << (svg/avg);
        }
    } while (0);

    do {
        COUT << "\n======= u64toh vs snprintf =======";
        t.restart();
        for (unsigned long long i = FLG_beg; i < FLG_end; i++) {
            snprintf(buf, 32, "0x%llx", i);
        }
        us = t.us();
        svg = static_cast<double>(us*1000) / (FLG_end - FLG_beg);
        COUT << "snprintf %llx " << FLG_beg << '~' << FLG_end << " done in " << us << " us, avg: " << svg << " ns";

        t.restart();
        for (uint64 i = FLG_beg; i < FLG_end; i++) {
            fast::u64toh(i, buf);
        }
        us = t.us();
        avg = static_cast<double>(us*1000) / (FLG_end - FLG_beg);
        COUT << "u64toh " << FLG_beg << '~' << FLG_end << ", done in " << us << " us, avg: " << avg << " ns";
        COUT << "-> speedup: " << (svg/avg);
    } while (0);

    do {
        COUT << "\n======= u32toh vs snprintf =======";
        if (FLG_beg <= MAX_UINT32 - 100000) {
            uint32 u32beg = (uint32) FLG_beg;
            uint32 u32end = (uint32) FLG_end;

            t.restart();
            for (uint32 i = u32beg; i < u32end; i++) {
                fast::u32toh(i, buf);
            }
            us = t.us();
            avg = static_cast<double>(us*1000) / (FLG_end - FLG_beg);
            COUT << "u32toh " << FLG_beg << '~' << FLG_end << ", done in " << us << " us, avg: " << avg << " ns";
            COUT << "-> speedup: " << (svg/avg);
        }
    } while (0);

    do {
        COUT << "\n======= dtoa vs snprintf =======";
        double f = -3.14159;

        t.restart();
        for (int i = 0; i < 100000; i++) {
            r = snprintf(buf, 32, "%.7g", f);
        }
        us = t.us();
        svg = static_cast<double>(us*1000) / 100000;
        COUT << "snprintf double 10w times, done in " << us << " us, avg: " << svg << " ns";

        t.restart();
        for (int i = 0; i < 100000; i++) {
            r = fast::dtoa(f, buf);
        }
        us = t.us();
        avg = static_cast<double>(us*1000) / 100000;
        COUT << "fast::dtoa 10w times, done in " << us << " us, avg: " << avg << " ns";
        COUT << "-> speedup: " << (svg/avg);
        COUT << buf << "  r: " << r;
    } while (0);

    return 0;
}
