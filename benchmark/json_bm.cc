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

    fs::file bob("bob.json", 'r');
    fastring kk = bob.read(bob.size());
    COUT << "bob size: " << bob.size();

    Json bjs = json::parse(kk);
    auto yy = bjs.pretty();
    COUT << (yy == kk);

    auto zz = bjs.str();
    Json mjs = json::parse(zz);
    auto vv = mjs.pretty();
    COUT << (vv == kk);
    COUT << vv;
    return 0;

    fs::file f(FLG_s.c_str(), 'r');
    if (!f) {
        COUT << "open file failed: " << FLG_s;
        return 0;
    }

    fastring s = f.read(f.size());
    COUT << "json file size: " << s.size();
    if (FLG_minimal) {
        Json x = json::parse(s);
        s = x.str();
        COUT << "minimal json file size: " << s.size();
    }

    double tco, tsimd, trap;
    Timer timer;
    {
        {
            Json v = json::parse(s);
            COUT << v.get("search_metadata", "count").as_int() << " results.";
            COUT << v.get("statuses", 0, "metadata", "result_type").as_string();
            COUT << v["statuses"][0]["in_reply_to_user_id_str"].as_string();
            timer.restart();
            fastring s = v.str();
            COUT << "co/json str() time:" << timer.us();
            COUT << "str() size: " << s.size();
            fs::file cox("cox.json", 'w');
            cox.write(v.pretty());
            cox.close();
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
            COUT << v["statuses"][0]["in_reply_to_user_id_str"].GetString();
            timer.restart();
            std::string s = rapidjson::str(v);
            COUT << "rapidjson str() time:" << timer.us();
            COUT << "str() size: " << s.size();
            fs::file xx("xx.json", 'w');
            xx.write(rapidjson::pretty(v));
            xx.close();
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
            std::cout << v["statuses"].at(0)["in_reply_to_user_id_str"] << '\n';
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
