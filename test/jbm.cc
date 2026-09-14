#include "co/all.h"

DEF_int32(n, 1000, "times");
DEF_string(s, "twitter.json", "json path");
DEF_bool(minimal, false, "");

int main(int argc, char** argv) {
    flag::parse(argc, argv);

    fs::file f(FLG_s.c_str(), 'r');
    if (!f) {
        co::println("open file failed: ", FLG_s);
        return 0;
    }

    co::string s = f.read(f.size());
    co::println("json file size: ", s.size());
    if (FLG_minimal) {
        json::any x = json::parse(s);
        s = x.str();
        co::println("minimal json file size: ", s.size());
    }

    int64 us;
    double tco;
    co::timer timer;
    {
        {
            json::any v = json::parse(s);
            co::println(v["search_metadata"]["count"].as_int(), " results.");
            co::println(v["statuses"][0]["metadata"]["result_type"].as_string());
            { co::string xx = v.str(); }
            timer.restart();
            co::string s = v.str();
            us = timer.us();
            co::println("co/json str() time:", us, " us");
            co::println("str() size: ", s.size());
        }
        timer.restart();
        for (int i = 0; i < FLG_n; ++i) {
            json::any v = json::parse(s);
        }
        us = timer.us();
        co::println( "co/json parse time: ", (tco = us * 1.0 / FLG_n), " us");
    }

    return 0;
}
