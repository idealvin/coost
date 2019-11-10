#include "rapidjson.h"
#include "base/time.h"
#include <string>
#include <iostream>

int main(int argc, char** argv) {
    rapidjson::Document v;
    v.SetObject();
    auto& alloc = v.GetAllocator();

    v.AddMember("name", "vin", alloc);
    v.AddMember("age", 23, alloc);
#if 0
    v.AddMember("hello11111", "hello world", alloc);
    v.AddMember("hello22222", "hello world", alloc);
    v.AddMember("hello33333", "hello world", alloc);
    v.AddMember("hello44444", "hello world", alloc);
    v.AddMember("hello55555", "hello world", alloc);
#endif

    rapidjson::Value a;
    a.SetArray();
    a.PushBack(1, alloc);
    a.PushBack(2, alloc);
    a.PushBack(3, alloc);

    v.AddMember("num", a, alloc);

    rapidjson::Value b;
    b.SetArray();
    b.PushBack(1, alloc);
    b.PushBack(2, alloc);
    b.PushBack(3, alloc);

    rapidjson::Value o;
    o.SetObject();
    o.AddMember("o1", 3.14, alloc);
    o.AddMember("o2", "good", alloc);
    o.AddMember("o3", b, alloc);

    v.AddMember("o", o, alloc);

    std::string s = rapidjson::str(v);
    std::cout << s << std::endl;

    rapidjson::Document u;
    bool ok = rapidjson::parse(s, u);
    if (!ok) {
        std::cout << "parse error.." << std::endl;
        return -1;
    }

    std::cout << rapidjson::str(u) << std::endl;
    std::cout << rapidjson::pretty(u) << std::endl;

    if (u.HasMember("name")) {
        std::cout << "name: " << u["name"].GetString() << std::endl;
    }

    int n = 100000;

    int64 beg = now::us();
    for (int i = 0; i < n; ++i) {
        rapidjson::Document xx;
        rapidjson::parse(s, xx);
    }
    int64 end = now::us();

    std::cout << "parse average time used: "
              << (end - beg) * 1.0/ n << "us" << std::endl;

    std::string xs;
    beg = now::us();
    for (int i = 0; i < n; ++i) {
        xs = rapidjson::str(u);
    }
    end = now::us();

    std::cout << "stringify average time used: "
              << (end - beg) * 1.0 / n << "us" << std::endl;

    beg = now::us();
    for (int i = 0; i < n; ++i) {
        xs = rapidjson::pretty(u);
    }
    end = now::us();

    std::cout << "pretty average time used: "
              << (end - beg) * 1.0 / n << "us" << std::endl;

    return 0;
}
