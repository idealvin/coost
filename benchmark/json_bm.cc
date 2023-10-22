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

    fs::file f(FLG_s.c_str(), 'r');
    if (!f) {
        cout << "open file failed: " << FLG_s << '\n';
        return 0;
    }

    fastring s = f.read(f.size());
    cout << "json file size: " << s.size() << '\n';
    if (FLG_minimal) {
        co::Json x = json::parse(s);
        s = x.str();
        cout << "minimal json file size: " << s.size() << '\n';
    }

    int64 us;
    double tco, tsimd, trap;
    co::Timer timer;
    {
        {
            co::Json v = json::parse(s);
            cout << v["search_metadata"]["count"].as_int() << " results.\n";
            cout << v["statuses"][0]["metadata"]["result_type"].as_string() << '\n';
            { fastring xx = v.str(); }
            timer.restart();
            fastring s = v.str();
            us = timer.us();
            cout << "co/json str() time:" << us << " us\n";
            cout << "str() size: " << s.size() << '\n';
        }
        timer.restart();
        for (int i = 0; i < FLG_n; ++i) {
            co::Json v = json::parse(s);
        }
        us = timer.us();
        cout << "co/json parse time: " << (tco = us * 1.0 / FLG_n) << " us\n";
    }

    {
        {
            rapidjson::Document v;
            rapidjson::parse(s.c_str(), v);
            cout << v["search_metadata"]["count"].GetInt() << " results.\n";
            cout << v["statuses"][0]["metadata"]["result_type"].GetString() << '\n';
            { fastring xx = rapidjson::str(v); }
            timer.restart();
            std::string s = rapidjson::str(v);
            us = timer.us();
            cout << "rapidjson str() time:" << us << " us\n";
            cout << "str() size: " << s.size() << '\n';
        }
        timer.restart();
        for (int i = 0; i < FLG_n; ++i) {
            rapidjson::Document v;
            rapidjson::parse(s.c_str(), v);
        }
        us = timer.us();
        cout << "rapidjson parse time: " << (us * 1.0 / FLG_n) << " us\n";
    }

    {
        {
            simdjson::dom::parser parser;
            simdjson::dom::element v;
            simdjson::dom::element x;
            parser.parse(s.data(), s.size()).get(v);
            parser.parse(s.data(), s.size()).get(x);
            std::cout << v["search_metadata"]["count"] << " results." << '\n';
            std::cout << v["statuses"].at(0)["metadata"]["result_type"] << '\n';
            std::cout << v["statuses"].at(0)["in_reply_to_user_id_str"] << '\n';

            {
                std::ostringstream oss;
                oss << x;
                std::string xx = oss.str();
            }
            std::ostringstream oss;
            timer.restart();
            oss << v;
            std::string s = oss.str();
            us = timer.us();
            cout << "simdjson str() time:" << us << " us\n";
            cout << "simdjson str() size: " << s.size() << '\n';
        }
        timer.restart();
        for (int i = 0; i < FLG_n; ++i) {
            simdjson::dom::parser parser;
            simdjson::dom::element v;
            parser.parse(s.data(), s.size()).get(v);
        }
        us = timer.us();
        std::cout << "simdjson parse time: " << (tsimd = us * 1.0 / FLG_n) << " us" << std::endl;
    }

    cout << ">>> " << (tco / tsimd) << '\n';

    return 0;
}
