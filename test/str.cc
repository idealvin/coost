#include "co/fastring.h"
#include "co/str.h"
#include "co/flag.h"
#include "co/benchmark.h"
#include "co/cout.h"

DEF_string(s, "", "string for search test");

BM_group(copy) {
    std::string ss;
    fastring fs;

    BM_add(std::string="")(
        ss = "";
    )
    BM_use(ss);

    BM_add(fastring="")(
        fs = "";
    )
    BM_use(fs);

    BM_add(=std::string())(
        ss = std::string();
    )
    BM_use(ss);

    BM_add(=fastring())(
        fs = fastring();
    )
    BM_use(fs);

    BM_add(std::string=helloagain)(
        ss = "helloagain";
    )
    BM_use(ss);

    BM_add(fastring=helloagain)(
        fs = "helloagain";
    )
    BM_use(fs);

    fs = fastring(8191, 'x');
    ss = std::string(8191, 'x');

    BM_add(std::string=.c_str()+1)(
        ss = ss.c_str() + 1;
    )
    BM_use(ss);

    BM_add(fastring=.c_str()+1)(
        fs = fs.c_str() + 1;
    )
    BM_use(fs);
}

BM_group(find) {
    size_t p;
    std::string s(FLG_s.data(), FLG_s.size());
    BM_add(std::string::find)(
        p = s.find("xy");
    )
    BM_use(p);
    co::print("p: ", p);

    BM_add(fastring::find)(
        p = FLG_s.find("xy");
    )
    BM_use(p);
    co::print("p: ", p);
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    if (FLG_s.empty()) {
        FLG_s.append(256, 'x').append(128, 'y').append(128, 'z');
    }
    bm::run_benchmarks();
    return 0;
}
