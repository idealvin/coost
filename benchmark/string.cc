
#include "co/benchmark.h"
#include "co/flag.h"

DEF_string(s, "", "string for search test");

BM_group(string) {
    if (FLG_s.empty()) {
        FLG_s.append(256, 'x').append(128, 'y').append(128, 'z');
    }

    std::string ss;
    co::string cs;

    BM_sub_group_begin;
    BM_add(std::string="") {
        ss = "";
    }
    BM_use(ss);

    BM_add(co::string="") {
        cs = "";
    }
    BM_use(cs);

    BM_sub_group_begin;
    BM_add(=std::string()) {
        ss = std::string();
    }
    BM_use(ss);

    BM_add(=co::string()) {
        cs = co::string();
    }
    BM_use(cs);

    BM_sub_group_begin;
    BM_add(std::string=helloagain) {
        ss = "helloagain";
    }
    BM_use(ss);

    BM_add(co::string=helloagain) {
        cs = "helloagain";
    }
    BM_use(cs);

    BM_sub_group_begin;
    cs = co::string(8191, 'x');
    ss = std::string(8191, 'x');

    BM_add(std::string=.c_str()+1) {
        ss = ss.c_str() + 1;
    }
    BM_use(ss);

    BM_add(co::string=.c_str()+1) {
        cs = cs.c_str() + 1;
    }
    BM_use(cs);

    BM_sub_group_begin;
    size_t p;
    std::string s(FLG_s.data(), FLG_s.size());
    BM_add(std::string::find) {
        p = s.find("xy");
    }
    BM_use(p);

    BM_add(co::string::find) {
        p = FLG_s.find("xy");
    }
    BM_use(p);
}
