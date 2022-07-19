# Coost

English | [简体中文](readme_cn.md)

[![Linux Build](https://img.shields.io/github/workflow/status/idealvin/coost/Linux/master.svg?logo=linux)](https://github.com/idealvin/coost/actions?query=workflow%3ALinux)
[![Windows Build](https://img.shields.io/github/workflow/status/idealvin/coost/Windows/master.svg?logo=windows)](https://github.com/idealvin/coost/actions?query=workflow%3AWindows)
[![Mac Build](https://img.shields.io/github/workflow/status/idealvin/coost/macOS/master.svg?logo=apple)](https://github.com/idealvin/coost/actions?query=workflow%3AmacOS)
[![Release](https://img.shields.io/github/release/idealvin/coost.svg)](https://github.com/idealvin/coost/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)


**A tiny boost library in C++11.**



## 0. Introduction

**Coost** is an elegant and efficient cross-platform C++ base library, it is not as heavy as [boost](https://www.boost.org/), but still provides enough powerful features:

- Command line and config file parser (flag)
- High performance log library (log)
- Unit testing framework (unitest)
- go-style coroutine
- Coroutine-based network programming framework
- Efficient JSON library
- JSON based RPC framework
- God-oriented programming
- Atomic operation (atomic)
- Random number generator (random)
- Efficient stream (fastream)
- Efficient string (fastring)
- String utility (str)
- Time library (time)
- Thread library (thread)
- Fast memory allocator
- LruMap
- hash library
- path library
- File system operations (fs)
- System operations (os)
 
Coost, formerly known as **cocoyaxi (co for short)**, for fear of exposing too much information and causing the Namake planet to suffer attack from **[the Dark Forest](https://en.wikipedia.org/wiki/The_Dark_Forest)**, was renamed Coost, which means a more lightweight C++ base library than boost.

> It was said that about xx light-years from the Earth, there is a planet named **Namake**. Namake has three suns, a large one and two small ones. The Namakians make a living by programming. They divide themselves into nine levels according to their programming level, and the three lowest levels will be sent to other planets to develop programming technology. These wandering Namakians must collect at least **10,000 stars** through a project before they can return to Namake.
> 
> Several years ago, two Namakians, [ruki](https://github.com/waruqi) and [alvin](https://github.com/idealvin), were dispatched to the Earth. To go back to the Namake planet as soon as possible, ruki has developed a powerful build tool [xmake](https://github.com/xmake-io/xmake), whose name is taken from Namake. At the same time, alvin has developed a tiny boost library [coost](https://github.com/idealvin/coost), whose original name cocoyaxi is taken from the Cocoyaxi village where ruki and alvin lived on Namake.



## 1. Sponsor

Coost needs your help. If you are using it or like it, you may consider becoming a sponsor. Thank you very much!

- [Github Sponsors](https://github.com/sponsors/idealvin)
- [A cup of coffee](https://coostdocs.github.io/en/about/sponsor/)

**Special Sponsors**

Coost is specially sponsored by the following companies, thank you very much!

<a href="https://www.oneflow.org/index.html">
<img src="https://coostdocs.github.io/images/sponsor/oneflow.png" width="175" height="125">
</a>



## 2. Documents

- English: [github](https://coostdocs.github.io/en/about/co/) | [gitee](https://coostdocs.gitee.io/en/about/co/)
- 简体中文: [github](https://coostdocs.github.io/cn/about/co/) | [gitee](https://coostdocs.gitee.io/cn/about/co/)




## 3. Core features


### 3.0 God-oriented programming

```cpp
#include "co/god.h"

void f() {
    god::bless_no_bugs();
}
```



### 3.1 co/flag

[co/flag](https://coostdocs.github.io/en/co/flag/) is a command line and config file parser. It is similar to [gflags](https://github.com/gflags/gflags), but more powerful:
- Support input parameters from command line and config file.
- Support automatic generation of the config file.
- Support flag aliases.
- Flag of integer type, the value can take a unit `k,m,g,t,p`.

```cpp
#include "co/flag.h"
#include "co/cout.h"

DEF_bool(x, false, "bool x");
DEF_int32(i, 0, "...");
DEF_string(s, "hello world", "string");

int main(int argc, char** argv) {
    flag::init(argc, argv);
    COUT << "x: " << FLG_x;
    COUT << "i: " << FLG_i;
    COUT << FLG_s << "|" << FLG_s.size();
    return 0;
}
```

In the above example, the macros start with `DEF_` define 3 flags. Each flag corresponds to a global variable, whose name is `FLG_` plus the flag name. After building, the above code can run as follows:

```sh
./xx                    # Run with default configs
./xx -x -s good         # x = true, s = "good"
./xx -i 4k -s "I'm ok"  # i = 4096, s = "I'm ok"

./xx -mkconf            # Automatically generate a config file: xx.conf
./xx xx.conf            # run with a config file
./xx -conf xx.conf      # Same as above
```



### 3.2 co/log

[co/log](https://coostdocs.github.io/en/co/log/) is a high-performance and memory-friendly log library, which nearly needs no memory allocation.

co/log supports two types of logs: one is the level log, which divides the logs into 5 levels: debug, info, warning, error and fatal. **Printing a fatal log will terminate the program**; the other one is TLOG, logs are classified by topic, and logs of different topics are written to different files.

```cpp
DLOG << "hello " << 23;  // debug
LOG << "hello " << 23;   // info
WLOG << "hello " << 23;  // warning
ELOG << "hello " << 23;  // error
FLOG << "hello " << 23;  // fatal
TLOG("xx") << "s" << 23; // topic log
```

co/log also provides a series of `CHECK` macros, which is an enhanced version of `assert`, and they will not be cleared in debug mode.

```cpp
void* p = malloc(32);
CHECK(p != NULL) << "malloc failed..";
CHECK_NE(p, NULL) << "malloc failed..";
```

When the CHECK assertion failed, co/log will print the function call stack information, and then terminate the program. On linux and macosx, make sure you have installed [libbacktrace](https://github.com/ianlancetaylor/libbacktrace) on your system.

![stack](https://coostdocs.github.io/images/stack.png)

co/log is very fast. The following are some test results, for reference only:

- co/log vs glog (single thread)

  | platform | google glog | co/log |
  | ------ | ------ | ------ |
  | win2012 HHD | 1.6MB/s | 180MB/s |
  | win10 SSD | 3.7MB/s | 560MB/s |
  | mac SSD | 17MB/s | 450MB/s |
  | linux SSD | 54MB/s | 1023MB/s |

- [co/log vs spdlog](https://github.com/idealvin/co/tree/benchmark) (Linux)

  | threads | total logs | co/log time(seconds) | spdlog time(seconds)|
  | ------ | ------ | ------ | ------ |
  | 1 | 1000000 | 0.096445 | 2.006087 |
  | 2 | 1000000 | 0.142160 | 3.276006 |
  | 4 | 1000000 | 0.181407 | 4.339714 |
  | 8 | 1000000 | 0.303968 | 4.700860 |



### 3.3 co/unitest

[co/unitest](https://coostdocs.github.io/en/co/unitest/) is a simple and easy-to-use unit test framework. Many components in co use it to write unit test code, which guarantees the stability of co.

```cpp
#include "co/unitest.h"
#include "co/os.h"

namespace test {
    
DEF_test(os) {
    DEF_case(homedir) {
        EXPECT_NE(os::homedir(), "");
    }

    DEF_case(cpunum) {
        EXPECT_GT(os::cpunum(), 0);
    }
}
    
} // namespace test
```

The above is a simple example. The `DEF_test` macro defines a test unit, which is actually a function (a method in a class). The `DEF_case` macro defines test cases, and each test case is actually a code block. The main function is simple as below:

```cpp
#include "co/unitest.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    unitest::run_all_tests();
    return 0;
}
```

[unitest](https://github.com/idealvin/coost/tree/master/unitest) contains the unit test code in coost. Users can run unitest with the following commands:

```sh
xmake r unitest      # Run all test cases
xmake r unitest -os  # Run test cases in the os unit
```



### 3.4 JSON

[co/json](https://coostdocs.github.io/cn/co/json/) is a fast JSON library, and it is quite easy to use.

```cpp
// {"a":23,"b":false,"s":"xx","v":[1,2,3],"o":{"xx":0}}
Json x = {
    { "a", 23 },
    { "b", false },
    { "s", "xx" },
    { "v", {1,2,3} },
    { "o", {
        {"xx", 0}
    }},
};

// equal to x
Json y = Json()
    .add_member("a", 23)
    .add_member("b", false)
    .add_member("s", "xx")
    .add_member("v", Json().push_back(1).push_back(2).push_back(3))
    .add_member("o", Json().add_member("xx", 0));

x.get("a").as_int();       // 23
x.get("s").as_string();    // "xx"
x.get("v", 0).as_int();    // 1
x.get("v", 2).as_int();    // 3
x.get("o", "xx").as_int(); // 0
```

- [co/json vs rapidjson](https://github.com/idealvin/coost/tree/benchmark/benchmark) (Linux)

  |  | parse | stringify | parse(minimal) | stringify(minimal) |
  | ------ | ------ | ------ | ------ | ------ |
  | rapidjson | 1270 us | 2106 us | 1127 us | 1358 us |
  | co/json | 1005 us | 920 us | 788 us | 470 us |



### 3.5 Coroutine

co has implemented a [go-style](https://github.com/golang/go) coroutine, which has the following features:

- Support multi-thread scheduling, the default number of threads is the number of system CPU cores.
- Shared stack, coroutines in the same thread share several stacks (the default size is 1MB), and the memory usage is low. Simple test on Linux shows that 10 millions of coroutines only take 2.8G of memory (for reference only).
- There is a flat relationship between coroutines, and new coroutines can be created from anywhere (including in coroutines).
- Support system API hook (Windows/Linux/Mac), you can directly use third-party network library in coroutine.

- Support coroutine lock [co::Mutex](https://coostdocs.github.io/en/co/coroutine/#comutex), coroutine synchronization event [co::Event](https://coostdocs.github.io/en/co/coroutine/#coevent).
- Support channel and waitgroup in golang: [co::Chan](https://coostdocs.github.io/en/co/coroutine/#cochan), [co::WaitGroup](https://coostdocs.github.io/en/co/coroutine/#cowaitgroup).
- Support coroutine pool [co::Pool](https://coostdocs.github.io/en/co/coroutine/#copool) (no lock, no atomic operation).


```cpp
#include "co/co.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);

    go(ku);            // void ku();
    go(f, 7);          // void f(int);
    go(&T::g, &o);     // void T::g(); T o;
    go(&T::h, &o, 7);  // void T::h(int); T o;
    go([](){
        LOG << "hello go";
    });

    co::sleep(32); // sleep 32 ms
    return 0;
}
```

In the above code, the coroutines created by `go()` will be evenly distributed to different scheduling threads. Users can also control the scheduling of coroutines by themselves:

```cpp
// run f1 and f2 in the same scheduler
auto s = co::next_scheduler();
s->go(f1);
s->go(f2);

// run f in all schedulers
for (auto& s : co::schedulers()) {
    s->go(f);
}
```



### 3.6 network programming

co provides a set of coroutineized [socket APIs](https://coostdocs.github.io/en/co/coroutine/#coroutineized-socket-api), most of them are consistent with the native socket APIs in form, with which, you can easily write high-performance network programs in a synchronous manner.

In addition, co has also implemented higher-level network programming components, including [TCP](https://coostdocs.github.io/en/co/net/tcp/), [HTTP](https://coostdocs.github.io/en/co/net/http/) and [RPC](https://coostdocs.github.io/en/co/net/rpc/) framework based on [JSON](https://coostdocs.github.io/en/co/json/), they are IPv6-compatible and support SSL at the same time, which is more convenient than socket APIs.


- **RPC server**

```cpp
int main(int argc, char** argv) {
    flag::init(argc, argv);

    rpc::Server()
        .add_service(new xx::HelloWorldImpl)
        .start("127.0.0.1", 7788, "/xx");

    for (;;) sleep::sec(80000);
    return 0;
}
```

**co/rpc also supports HTTP protocol**, you can use the POST method to call the RPC service:

```sh
curl http://127.0.0.1:7788/xx --request POST --data '{"api":"ping"}'
```


- **Static web server**

```cpp
#include "co/flag.h"
#include "co/http.h"

DEF_string(d, ".", "root dir"); // docroot for the web server

int main(int argc, char** argv) {
    flag::init(argc, argv);
    so::easy(FLG_d.c_str()); // mum never have to worry again
    return 0;
}
```


- **HTTP server**

```cpp
void cb(const http::Req& req, http::Res& res) {
    if (req.is_method_get()) {
        if (req.url() == "/hello") {
            res.set_status(200);
            res.set_body("hello world");
        } else {
            res.set_status(404);
        }
    } else {
        res.set_status(405); // method not allowed
    }
}

// http
http::Server().on_req(cb).start("0.0.0.0", 80);

// https
http::Server().on_req(cb).start(
    "0.0.0.0", 443, "privkey.pem", "certificate.pem"
);
```


- **HTTP client**

```cpp
void f() {
    http::Client c("https://github.com");

    c.get("/");
    LOG << "response code: "<< c.status();
    LOG << "body size: "<< c.body().size();
    LOG << "Content-Length: "<< c.header("Content-Length");
    LOG << c.header();

    c.post("/hello", "data xxx");
    LOG << "response code: "<< c.status();
}

go(f);
```




## 4. Code composition

- [include](https://github.com/idealvin/coost/tree/master/include)

  Header files of co.

- [src](https://github.com/idealvin/coost/tree/master/src)

  Source files of co, built as libco.

- [test](https://github.com/idealvin/coost/tree/master/test)

  Some test code, each `.cc` file will be compiled into a separate test program.

- [unitest](https://github.com/idealvin/coost/tree/master/unitest)

  Some unit test code, each `.cc` file corresponds to a different test unit, and all code will be compiled into a single test program.

- [gen](https://github.com/idealvin/coost/tree/master/gen)

  A code generator for the RPC framework.




## 5. Building

### 5.1 Compilers required

To build co, you need a compiler that supports C++11:

- Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
- Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
- Windows: [vs2015+](https://visualstudio.microsoft.com/)


### 5.2 Build with xmake

co recommends using [xmake](https://github.com/xmake-io/xmake) as the build tool.


#### 5.2.1 Quick start

```sh
# All commands are executed in the root directory of co (the same below)
xmake      # build libco by default
xmake -a   # build all projects (libco, gen, test, unitest)
```


#### 5.2.2 Build shared library

```sh
xmake f -k shared
xmake -v
```

#### 5.2.3 Build with mingw

```sh
xmake f -p mingw
xmake -v
```


#### 5.2.4 Enable HTTP/SSL features

```sh
xmake f --with_libcurl=true --with_openssl=true
xmake -v
```


#### 5.2.5 Install libco

```sh
# Install header files and libco by default.
xmake install -o pkg         # package related files to the pkg directory
xmake i -o pkg               # the same as above
xmake install -o /usr/local  # install to the /usr/local directory
```


#### 5.2.6 Install libco from xrepo

```sh
xrepo install -f "openssl=true,libcurl=true" coost
```



### 5.3 Build with cmake

[izhengfan](https://github.com/izhengfan) helped to provide cmake support, [SpaceIm](https://github.com/SpaceIm) improved it and made it perfect.


#### 5.3.1 Build libco

```sh
mkdir build && cd build
cmake ..
make -j8
```


#### 5.3.2 Build all projects

```sh
mkdir build && cd build
cmake .. -DBUILD_ALL=ON
make -j8
```


#### 5.3.3 Enable HTTP/SSL features

```sh
mkdir build && cd build
cmake .. -DWITH_LIBCURL=ON -DWITH_OPENSSL=ON
make -j8
```


#### 5.3.4 Build shared library

```sh
cmake .. -DBUILD_SHARED_LIBS=ON
make -j8
```


#### 5.3.5 Install libco from vcpkg

```sh
vcpkg install coost:x64-windows

# HTTP & SSL support
vcpkg install coost[libcurl,openssl]:x64-windows
```


#### 5.3.6 Install libco from conan

```sh
conan install coost
```


#### 5.3.7 Find coost in Cmake

```cmake
find_package(coost REQUIRED CONFIG)
target_link_libraries(userTarget coost::co)
```




## 6. License

The MIT license. coost contains codes from some other projects, which have their own licenses, see details in [LICENSE.md](https://github.com/idealvin/coost/blob/master/LICENSE.md).




## 7. Special thanks

- The code of [co/context](https://github.com/idealvin/coost/tree/master/src/co/context) is from [tbox](https://github.com/tboox/tbox) by [ruki](https://github.com/waruqi), special thanks!
- The early English documents of co are translated by [Leedehai](https://github.com/Leedehai) and [daidai21](https://github.com/daidai21), special thanks!
- [ruki](https://github.com/waruqi) has helped to improve the xmake building scripts, thanks in particular!
- [izhengfan](https://github.com/izhengfan) provided cmake building scripts, thank you very much!
- [SpaceIm](https://github.com/SpaceIm) has improved the cmake building scripts, and provided support for `find_package`. Really great help, thank you!
