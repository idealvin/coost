#include "co/fastring.h"
#include "co/str.h"
#include "co/benchmark.h"

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

int main(int argc, char** argv) {
    bm::run_benchmarks();
    return 0;
}
