# coost

[简体中文](readme_cn.md) | English  
[![Linux](https://img.shields.io/github/actions/workflow/status/idealvin/coost/linux.yml?branch=master&label=Linux)](https://github.com/idealvin/coost/actions?query=workflow%3ALinux)
[![Mac](https://img.shields.io/github/actions/workflow/status/idealvin/coost/macos.yml?branch=master&label=Mac)](https://github.com/idealvin/coost/actions?query=workflow%3AmacOS)
[![Windows](https://img.shields.io/github/actions/workflow/status/idealvin/coost/win.yml?branch=master&label=Windows)](https://github.com/idealvin/coost/actions?query=workflow%3AWindows)
[![Windows-arm64](https://img.shields.io/github/actions/workflow/status/idealvin/coost/win_arm64.yml?branch=master&label=Windows-arm64)](https://github.com/idealvin/coost/actions?query=workflow%3AWindows-arm64)
[![FreeBSD](https://img.shields.io/github/actions/workflow/status/idealvin/coost/freebsd.yml?branch=master&label=FreeBSD)](https://github.com/idealvin/coost/actions?query=workflow%3AFreeBSD)
[![Release](https://img.shields.io/github/release/idealvin/coost.svg)](https://github.com/idealvin/coost/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)



**[A tiny, minimalist Swiss Army knife for C++.](https://github.com/idealvin/coost)**


## Introduction

**[coost](https://github.com/idealvin/coost)** is a lightweight, high-performance, easy-to-use C++ base library. It supports Linux, macOS, Windows, FreeBSD, and CPU architectures such as x86, x64, ARM, and ARM64.

**coost**, abbreviated as **co**, includes go-style coroutines, networking, logging, config parsing, unit testing, benchmarking, memory allocator, and other components. Its goal is to make C++ programming simple and enjoyable.


## Commercial Support

coost remains open source. If you use coost in production and need in-depth support such as custom development, architecture adaptation (RISC-V / MIPS), coroutine hooks, performance optimization, or team training, the author offers paid commercial support to reduce risk and save time.

For service packages, pricing, and process, see:

👉 [coost Commercial Support](https://coostdocs.github.io/en/about/support/)

Contact the author via [GitHub Issues](https://github.com/idealvin/coost/issues) or email [idealvin@qq.com](mailto:idealvin@qq.com). A free initial diagnosis is available to assess scope and feasibility.



## Documentation

**The documentation currently lags behind the latest version of coost. Please refer to the [latest code](https://github.com/idealvin/coost) and the [include/co](https://github.com/idealvin/coost/tree/master/include/co) headers for the most accurate information.**

- [English](https://coostdocs.github.io/en/about/co/)
- [简体中文](https://coostdocs.github.io/cn/about/co/)



## Core Components

### flag

**[flag](https://github.com/idealvin/coost/blob/master/include/co/flag.h)** is a command-line argument and config file parsing library. It supports flag aliases, automatic config file generation, and more.

```cpp
#include "co/flag.h"
#include "co/print.h"

DEF_bool(x, false, "Comment here");
DEF_int32(n, 0, "Comment here");
DEF_string(s, "hello world", "Comment here");

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    co::println("x: ", FLG_x);
    co::println("n: ", FLG_n);
    co::println("s: ", FLG_s);
    return 0;
}
```


### Coroutines

coost implements a coroutine mechanism similar to goroutine in Golang. It has the following features:

- Multi-thread scheduling. By default, the number of scheduling threads is the number of CPU cores.
- Shared stacks. Coroutines in the same thread share several stacks (default size 1MB), resulting in low memory usage.
- Supports coroutine synchronization events, coroutine locks, waitgroup.

```cpp
#include "co/co.h"
#include "co/flag.h"
#include "co/print.h"

int main(int argc, char** argv) {
    flag::parse(argc, argv);

    co::wait_group wg(2);

    go([wg](){
        co::println("hello world");
        wg.done();
    });

    go([wg](){
        co::println("hello again");
        wg.done();
    });

    wg.wait();
    return 0;
}
```



### Networking

coost provides a coroutine-based network programming framework:

- **[Socket API](https://github.com/idealvin/coost/blob/master/include/co/sock.h)**. It is similar in form to the system socket API. Users familiar with socket programming can easily write high-performance network programs in a synchronous style.
- [TCP](https://github.com/idealvin/coost/blob/master/include/co/tcp.h). Simple encapsulation of TCP server and client, compatible with IPv6.
- [RPC](https://github.com/idealvin/coost/blob/master/include/co/rpc.h). A simple RPC framework using JSON for serialization.



### Logging

**[log](https://github.com/idealvin/coost/blob/master/include/co/log.h)** is a high-performance logging component. It supports printing stack traces when the program crashes.

```cpp
#include "co/log.h"

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    log::info("hello ", 23);   // info
    log::warn("hello ", 23);   // warning
    log::error("hello ", 23);  // error
    log::check(1+1==2, "xx");  // runtime assertion; prints stack trace and exits on failure
    return 0;
}
```

log is very fast. Here are some test results:

| platform | glog | co/log | speedup |
| ------ | ------ | ------ | ------ |
| win2012 HDD | 1.6MB/s | 180MB/s | 112.5 |
| win10 SSD | 3.7MB/s | 560MB/s | 151.3 |
| mac SSD | 17MB/s | 450MB/s | 26.4 |
| linux SSD | 54MB/s | 1023MB/s | 18.9 |

The table above shows the write speed comparison between co/log and glog when continuously printing 1 million logs in a single thread. co/log is nearly two orders of magnitude faster than glog.

| threads | linux co/log | linux spdlog | win co/log | win spdlog | speedup |
| ------ | ------ | ------ | ------ | ------ | ------ |
| 1 | 0.087235 | 2.076172 | 0.117704 | 0.461156 | 23.8/3.9 |
| 2 | 0.183160 | 3.729386 | 0.158122 | 0.511769 | 20.3/3.2 |
| 4 | 0.206712 | 4.764238 | 0.316607 | 0.743227 | 23.0/2.3 |
| 8 | 0.302088 | 3.963644 | 0.406025 | 1.417387 | 13.1/3.5 |

The table above shows the time consumed to print 1 million logs with 1, 2, 4, and 8 threads respectively, in seconds. speedup is the performance improvement factor of co/log over spdlog on Linux and Windows.



### Unit Testing

**[unitest](https://github.com/idealvin/coost/blob/master/include/co/unitest.h)** is a simple and easy-to-use unit testing framework. Many components in coost use it to write [unit test code](https://github.com/idealvin/coost/tree/master/unitest), providing important assurance for coost's stability.

```cpp
#include "co/unitest.h"
#include "co/os.h"

DEF_test(os) {
    DEF_case(homedir) {
        EXPECT_NE(os::homedir(), "");
    }

    DEF_case(cpunum) {
        EXPECT_GT(os::cpunum(), 0);
    }
}

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    co::run_unitests();
    return 0;
}
```

The above is a simple example. The `DEF_test` macro defines a test unit, which is actually a function. The `DEF_case` macro defines a test case, and each test case is actually a code block.

The [coost/unitest](https://github.com/idealvin/coost/tree/master/unitest) directory contains the unit test code in coost. Run the following commands to build and run:

```sh
xmake b unitest
xmake r unitest      # run all unit test cases
xmake r unitest -os  # run only test cases in the os unit; os is the unit test name
```



### Benchmark

**[benchmark](https://github.com/idealvin/coost/blob/master/include/co/benchmark.h)** is a simple and easy-to-use performance benchmark framework.

```cpp
#include "co/benchmark.h"
#include "co/atomic.h"

BM_group(atomic) {
    int i = 0;

    BM_add(atomic_inc) {
        co::atomic_inc(&i);
    }
    BM_use(i);

    BM_add(atomic_dec) {
        co::atomic_dec(&i);
    }
    BM_use(i);
}
```

The [coost/benchmark](https://github.com/idealvin/coost/tree/master/benchmark) directory contains some performance test code. Run the following commands to build and run:

```sh
xmake b benchmark
xmake r benchmark        # run all benchmark code by default
xmake r benchmark -mem   # run only benchmark code in BM_group(mem)
```



### JSON

**[JSON](https://github.com/idealvin/coost/blob/master/include/co/json.h)** adopts a **fluent interface design**, making it easier to use.

```cpp
// {"a":23,"b":false,"s":"123","v":[1,2,3],"o":{"xx":0}}
json::any x = {
    { "a", 23 },
    { "b", false },
    { "s", "123" },
    { "v", {1,2,3} },
    { "o", {
        {"xx", 0}
    }},
};

// equal to x
json::any y = Json()
    .add_member("a", 23)
    .add_member("b", false)
    .add_member("s", "123")
    .add_member("v", Json().push_back(1).push_back(2).push_back(3))
    .add_member("o", Json().add_member("xx", 0));

x.get("a").as_int();       // 23
x.get("s").as_string();    // "123"
x.get("s").as_int();       // 123, string -> int
x.get("v", 0).as_int();    // 1
x.get("v", 2).as_int();    // 3
x.get("o", "xx").as_int(); // 0
```

Below is the performance comparison between co/json and rapidjson:

| os | co/json stringify | co/json parse | rapidjson stringify | rapidjson parse | speedup |
| ------ | ------ | ------ | ------ | ------ | ------ |
| win | 569 | 924 | 2089 | 2495 | 3.6/2.7 |
| mac | 783 | 1097 | 1289 | 1658 | 1.6/1.5 |
| linux | 468 | 764 | 1359 | 1070 | 2.9/1.4 |

The table above shows the average time for stringify and parse measured after minifying [twitter.json](https://raw.githubusercontent.com/simdjson/simdjson/master/jsonexamples/twitter.json), in microseconds (us). speedup is the performance improvement factor of co/json over rapidjson in stringify and parse.


## Code Structure

- [include](https://github.com/idealvin/coost/tree/master/include)  

  Header files of coost.

- [src](https://github.com/idealvin/coost/tree/master/src)  

  Source code of coost, which compiles into libco.

- [benchmark](https://github.com/idealvin/coost/tree/master/benchmark)  

  Performance benchmark code. Each `.cc` file corresponds to a different test unit, and all code is compiled into a single test program.

- [test](https://github.com/idealvin/coost/tree/master/test)  

  Test code. Each `.cc` file is compiled into a separate test program.

- [unitest](https://github.com/idealvin/coost/tree/master/unitest)  

  Unit test code. Each `.cc` file corresponds to a different test unit, and all code is compiled into a single test program.

- [gen](https://github.com/idealvin/coost/tree/master/gen)  

  Code generation tools.




## Build

### Compiler Requirements

**The latest version of coost requires a compiler that supports C++17**:

- Linux: [gcc](https://gcc.gnu.org/projects/cxx-status.html#cxx17)
- Mac: [clang](https://clang.llvm.org/cxx_status.html)
- Windows: [MSVC](https://visualstudio.microsoft.com/)


### Build with xmake

coost recommends [xmake](https://github.com/xmake-io/xmake) as the build tool.


#### Quick Start

```sh
# All commands are executed in the coost root directory, and this will not be repeated below
xmake       # build libco by default
xmake -a    # build all projects (libco, benchmark, gen, test, unitest)
```

#### Enable backtrace

On Linux and macOS, printing stack traces on crashes requires [libbacktrace](https://github.com/ianlancetaylor/libbacktrace). Newer versions of gcc on Linux already have a built-in backtrace library; on macOS it usually needs to be installed manually.

```sh
xmake f --with_backtrace=true
xmake b stack   # test/stack.cc
xmake r stack   # run stack test program
```



#### Install libco

```sh
xmake install -o pkg          # package and install to the pkg directory
xmake i -o pkg                # same as above
xmake install -o /usr/local   # install to /usr/local
```


### Build with cmake

#### Build libco

```sh
mkdir cmakebuild && cd cmakebuild
cmake ..
make -j8
```


#### Build all projects

```sh
mkdir cmakebuild && cd cmakebuild
cmake .. -DBUILD_ALL=ON -DCMAKE_INSTALL_PREFIX=/usr/local
make -j8
cd bin
./unitest  # run unit test program
```

#### Enable backtrace

```sh
mkdir cmakebuild && cd cmakebuild
cmake .. -DWITH_BACKTRACE=ON
make -j8
```



## License

The MIT license. coost includes code from some other projects, which may use different licenses. See [LICENSE.md](https://github.com/idealvin/coost/blob/master/LICENSE.md) for details.



## Special Thanks

- The related code of [context](https://github.com/idealvin/coost/tree/master/src/co/context) is taken from [ruki](https://github.com/waruqi)'s [tbox](https://github.com/tboox/tbox). ruki also helped improve the xmake build script. Special thanks!
- [izhengfan](https://github.com/izhengfan) provided the cmake build script. Special thanks!
- [SpaceIm](https://github.com/SpaceIm) improved the cmake build script and provided `find_package` support. Special thanks!
- [Leedehai](https://github.com/Leedehai) and [daidai21](https://github.com/daidai21) helped translate coost's Chinese reference documentation into English in the early days. Special thanks!
