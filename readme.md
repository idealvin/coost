# coost

English | [简体中文](readme_cn.md)  
[![Linux Build](https://img.shields.io/github/actions/workflow/status/idealvin/coost/linux.yml?branch=master&logo=linux)](https://github.com/idealvin/coost/actions?query=workflow%3ALinux)
[![Mac Build](https://img.shields.io/github/actions/workflow/status/idealvin/coost/macos.yml?branch=master&logo=apple)](https://github.com/idealvin/coost/actions?query=workflow%3AmacOS)
[![Windows Build](https://img.shields.io/github/actions/workflow/status/idealvin/coost/win.yml?branch=master&logo=windows)](https://github.com/idealvin/coost/actions?query=workflow%3AWindows)
[![FreeBSD Build](https://img.shields.io/github/actions/workflow/status/idealvin/coost/freebsd.yml?branch=master&logo=freebsd)](https://github.com/idealvin/coost/actions?query=workflow%3AFreeBSD)  
[![Release](https://img.shields.io/github/release/idealvin/coost.svg)](https://github.com/idealvin/coost/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)


**[A tiny, minimalist Swiss Army knife for C++.](https://github.com/idealvin/coost)**


## 0. Introduction

**[coost](https://github.com/idealvin/coost)** is a cross-platform C++ foundation library that combines **performance and ease of use**. Its goal is to be a powerful tool for C++ development, making C++ programming simple, relaxed, and enjoyable.

coost is abbreviated as **co**. Some people call it the Swiss Army knife of C++, and it has also been described as a small [boost](https://www.boost.org/). Compared with boost, coost is small and refined: **the static library built on Linux and macOS is only about 1 MB**, yet it packs a command-line and config-file parser (flag), a high-performance logging library (log), a unit testing framework (unitest), a benchmark framework (benchmark), a high-performance memory allocator, go-style coroutines (co), and a coroutine-based network programming and RPC framework, among many other powerful features.



## 1. Sponsorship and Paid Services

**[Buy the author a cup of tea](https://coostdocs.github.io/cn/about/sponsor/)**

Maintaining coost takes time and effort. If it has helped you, please consider sponsoring the project. If you need in-depth support such as custom development, architecture porting (Windows ARM64 / RISC-V / MIPS), coroutine hooks, or performance optimization, the author also offers the following paid services, including but not limited to:

- Custom feature development for coost;
- coost training;
- Porting coost to Windows ARM64, RISC-V, MIPS, and other architectures;
- Platform-specific coroutine hooks that allow third-party network libraries to be used directly inside coroutines;
- Technical consulting and training;
- Performance optimization;
- Solving complex technical problems.

If you are interested, please reach out via [GitHub Issues](https://github.com/idealvin/coost/issues) or email (idealvin@qq.com). Thank you!



## 2. Documentation

**The documentation currently lags behind the latest version of coost. Please refer to the [latest source code](https://github.com/idealvin/coost) and the [include/co](https://github.com/idealvin/coost/tree/master/include/co) headers.**

- [简体中文](https://coostdocs.github.io/cn/about/co/)
- [English](https://coostdocs.github.io/en/about/co/)




## 3. Core Components

### 3.1 flag

**[flag](https://coostdocs.github.io/cn/co/flag/)** is a command-line argument and config-file parser. Its usage is similar to gflags, but it is more powerful:

- Supports arguments from both the command line and a config file.
- Supports automatic generation of config files.
- Supports flag aliases.
- Integer flags accept unit suffixes `k, m, g, t, p`, case-insensitive.

See [test/flag.cc](https://github.com/idealvin/coost/blob/master/test/flag.cc) for usage examples.


### 3.2 log

**[log](https://coostdocs.github.io/cn/co/log/)** is a high-performance logging component that prints stack traces when the program crashes. It is very easy to use:

```cpp
#include "co/log.h"

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    log::debug("hello ", 23);  // debug
    log::info("hello ", 23);   // info
    log::warn("hello ", 23);   // warning
    log::error("hello ", 23);  // error
    log::fatal("hello", 23);   // fatal, terminates the program
    log::check(1+1==2, "xx");  // runtime assertion; on failure, prints a stack trace and exits
    return 0;
}
```

log is extremely fast. Here are some benchmark results:

| platform | glog | co/log | speedup |
| ------ | ------ | ------ | ------ |
| win2012 HDD | 1.6MB/s | 180MB/s | 112.5 |
| win10 SSD | 3.7MB/s | 560MB/s | 151.3 |
| mac SSD | 17MB/s | 450MB/s | 26.4 |
| linux SSD | 54MB/s | 1023MB/s | 18.9 |

The table above compares co/log and glog by measuring the write throughput when printing 1,000,000 log entries continuously in a single thread. co/log is nearly two orders of magnitude faster than glog.

| threads | linux co/log | linux spdlog | win co/log | win spdlog | speedup |
| ------ | ------ | ------ | ------ | ------ | ------ |
| 1 | 0.087235 | 2.076172 | 0.117704 | 0.461156 | 23.8/3.9 |
| 2 | 0.183160 | 3.729386 | 0.158122 | 0.511769 | 20.3/3.2 |
| 4 | 0.206712 | 4.764238 | 0.316607 | 0.743227 | 23.0/2.3 |
| 8 | 0.302088 | 3.963644 | 0.406025 | 1.417387 | 13.1/3.5 |

The table above shows the time (in seconds) required to [print 1,000,000 log entries using 1, 2, 4, and 8 threads respectively](https://github.com/idealvin/coost/tree/benchmark). The speedup column shows how many times faster co/log is compared with spdlog on Linux and Windows.



### 3.3 unitest

**[unitest](https://coostdocs.github.io/cn/co/unitest/)** is a simple and easy-to-use unit testing framework. Many components of coost use it to write unit tests, which provides important assurance for coost's stability.

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

The example above is simple. The `DEF_test` macro defines a test unit, which is essentially a function. The `DEF_case` macro defines a test case, which is essentially a code block.

The [unitest](https://github.com/idealvin/coost/tree/master/unitest) directory contains coost's unit test code. Build and run it with the following commands:

```sh
xmake b unitest
xmake r unitest      # run all unit test cases
xmake r unitest -os  # run only the test cases in the os unit; os is the unit name
```



### 3.4 JSON

**[Json](https://github.com/idealvin/coost/blob/master/include/co/json.h)** adopts a **fluent interface design**, making it more convenient to use.

```cpp
// {"a":23,"b":false,"s":"123","v":[1,2,3],"o":{"xx":0}}
co::Json x = {
    { "a", 23 },
    { "b", false },
    { "s", "123" },
    { "v", {1,2,3} },
    { "o", {
        {"xx", 0}
    }},
};

// equal to x
co::Json y = Json()
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

Below is a performance comparison between co/json and rapidjson:

| os | co/json stringify | co/json parse | rapidjson stringify | rapidjson parse | speedup |
| ------ | ------ | ------ | ------ | ------ | ------ |
| win | 569 | 924 | 2089 | 2495 | 3.6/2.7 |
| mac | 783 | 1097 | 1289 | 1658 | 1.6/1.5 |
| linux | 468 | 764 | 1359 | 1070 | 2.9/1.4 |

The table above shows the average time (in microseconds) for stringify and parse, measured after minifying [twitter.json](https://raw.githubusercontent.com/simdjson/simdjson/master/jsonexamples/twitter.json). The speedup column shows how many times faster co/json is compared with rapidjson for stringify and parse.



### 3.5 Coroutines

coost implements a coroutine mechanism similar to goroutines in Golang. It has the following features:

- Multi-threaded scheduling; the default number of scheduling threads equals the number of CPU cores.
- Shared stacks: coroutines in the same thread share several stacks (1 MB each by default), resulting in low memory usage.
- Coroutines are peers; new coroutines can be created anywhere (including inside a coroutine).
- Supports coroutine synchronization primitives such as events, locks, and waitgroups.

```cpp
#include "co/co.h"
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



### 3.6 Network Programming

coost provides a coroutine-based network programming framework:

- **[Coroutine-friendly socket API](https://coostdocs.github.io/cn/co/net/sock/)**, similar in form to the system socket API. Users familiar with socket programming can easily write high-performance network programs in a synchronous style.
- High-level components such as [TCP](https://coostdocs.github.io/cn/co/net/tcp/) and [RPC](https://coostdocs.github.io/cn/co/net/rpc/), with IPv6 support, easier to use than the socket API.



## 4. Code Layout

- [include](https://github.com/idealvin/coost/tree/master/include)

  coost header files.

- [src](https://github.com/idealvin/coost/tree/master/src)

  coost source code, which builds libco.

- [benchmark](https://github.com/idealvin/coost/tree/master/benchmark)

  Performance benchmark code. Each `.cc` file corresponds to a different test unit, and all code is compiled into a single test program.

- [test](https://github.com/idealvin/coost/tree/master/test)

  Test code. Each `.cc` file is compiled into a separate test program.

- [unitest](https://github.com/idealvin/coost/tree/master/unitest)

  Unit test code. Each `.cc` file corresponds to a different test unit, and all code is compiled into a single test program.

- [gen](https://github.com/idealvin/coost/tree/master/gen)

  Code generation tools.




## 5. Build

### 5.1 Compiler Requirements

**The latest version of coost requires a compiler that supports C++17**:

- Linux: [gcc](https://gcc.gnu.org/projects/cxx-status.html#cxx17)
- Mac: [clang](https://clang.llvm.org/cxx_status.html)
- Windows: [MSVC](https://visualstudio.microsoft.com/)


### 5.2 Build with xmake

coost recommends [xmake](https://github.com/xmake-io/xmake) as the build tool.


#### 5.2.1 Quick Start

```sh
# All commands are executed in the coost root directory; this is assumed below.
xmake       # build libco by default
xmake -a    # build all projects (libco, benchmark, gen, test, unitest)
```

#### 5.2.2 Enable backtrace

On Linux and macOS, printing stack traces when the program crashes requires [libbacktrace](https://github.com/ianlancetaylor/libbacktrace). Newer versions of gcc on Linux already include the backtrace library; on macOS it usually needs to be installed manually.

```sh
xmake f --with_backtrace=true
xmake b stack   # test/stack.cc
xmake r stack   # run the stack test program
```



#### 5.2.3 Install libco

```sh
xmake install -o pkg          # package and install to the pkg directory
xmake i -o pkg                # same as above
xmake install -o /usr/local   # install to /usr/local
```


### 5.3 Build with CMake

#### 5.3.1 Build libco

```sh
mkdir cmakebuild && cd cmakebuild
cmake ..
make -j8
```


#### 5.3.2 Build all projects

```sh
mkdir cmakebuild && cd cmakebuild
cmake .. -DBUILD_ALL=ON -DCMAKE_INSTALL_PREFIX=/usr/local
make -j8
cd bin
./unitest  # run the unit test program
```

#### 5.3.3 Enable backtrace

```sh
mkdir cmakebuild && cd cmakebuild
cmake .. -DWITH_BACKTRACE=ON
make -j8
```



## 6. License

The MIT license. coost includes code from some other projects, which may use different licenses. See [LICENSE.md](https://github.com/idealvin/coost/blob/master/LICENSE.md) for details.



## 7. Special Thanks

- The code related to [context](https://github.com/idealvin/coost/tree/master/src/co/context) is taken from [ruki](https://github.com/waruqi)'s [tbox](https://github.com/tboox/tbox); ruki also helped improve the xmake build scripts. Special thanks!
- [izhengfan](https://github.com/izhengfan) provided the CMake build scripts. Special thanks!
- [SpaceIm](https://github.com/SpaceIm) improved the CMake build scripts and provided `find_package` support. Special thanks!
- [Leedehai](https://github.com/Leedehai) and [daidai21](https://github.com/daidai21) helped translate the Chinese reference documentation into English in the early days. Special thanks!
