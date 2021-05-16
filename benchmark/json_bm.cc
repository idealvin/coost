#include "co/all.h"
#include <sstream>

#ifdef CO_SSE42
#include <nmmintrin.h>
#define RAPIDJSON_SSE42
#endif

#include "rapidjson.h"
#include "simdjson/simdjson.hpp"

DEF_int32(n, 1000, "times");
DEF_string(s, "twitter.json", "json path");
DEF_bool(minimal, false, "");

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    fs::file f(FLG_s.c_str(), 'r');
    if (!f) {
        COUT << "open file failed: " << FLG_s;
        return 0;
    }

    fastring s = f.read(f.size());
    COUT << "s.size(): " << s.size();
    if (FLG_minimal) {
        Json x = json::parse(s);
        s = x.str();
        COUT << "s.size(): " << s.size();
    }

    double tco, tsimd, trap;
    Timer timer;
    {
        {
            Json v = json::parse(s);
            COUT << v["search_metadata"]["count"] << " results.";
            COUT << v["statuses"][0]["metadata"]["result_type"].get_string();
            timer.restart();
            fastring s = v.str();
            COUT << "co/json str() time:" << timer.us();
            COUT << "str() size: " << s.size();
        }
        timer.restart();
        for (int i = 0; i < FLG_n; ++i) {
            Json v = json::parse(s);
        }
        COUT << "co/json parse time: " << (tco = timer.us() * 1.0 / FLG_n) << " us";
    }

    {
        {
            rapidjson::Document v;
            rapidjson::parse(s.c_str(), v);
            COUT << v["search_metadata"]["count"].GetInt() << " results.";
            COUT << v["statuses"][0]["metadata"]["result_type"].GetString();
            timer.restart();
            std::string s = rapidjson::str(v);
            COUT << "rapidjson str() time:" << timer.us();
            COUT << "str() size: " << s.size();
        }
        timer.restart();
        for (int i = 0; i < FLG_n; ++i) {
            rapidjson::Document v;
            rapidjson::parse(s.c_str(), v);
        }
        COUT << "rapidjson parse time: " << (timer.us() * 1.0 / FLG_n) << " us";
    }

    {
        {
            simdjson::dom::parser parser;
            simdjson::dom::element v;
            parser.parse(s.data(), s.size()).get(v);
            std::cout << v["search_metadata"]["count"] << " results." << '\n';
            std::cout << v["statuses"].at(0)["metadata"]["result_type"] << '\n';
            std::ostringstream oss;
            timer.restart();
            oss << v;
            std::string s = oss.str();
            COUT << "simdjson str() time:" << timer.us();
            COUT << "simdjson str() size: " << s.size();
        }
        timer.restart();
        for (int i = 0; i < FLG_n; ++i) {
            simdjson::dom::parser parser;
            simdjson::dom::element v;
            parser.parse(s.data(), s.size()).get(v);
        }
        std::cout << "simdjson parse time: " << (tsimd = timer.us() * 1.0 / FLG_n) << " us" << std::endl;
    }

    COUT << ">>> " << (tco / tsimd);

    return 0;
}
