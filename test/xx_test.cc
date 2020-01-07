#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include "test.h"
#include "base/def.h"
#include "base/log.h"
#include "base/str.h"
#include "base/fastream.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/co.h"
#include "base/json.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    COUT << "hello world";

    // do something here?

    return 0;
}
