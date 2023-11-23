# Coost

English | [简体中文](readme_cn.md)

[![Linux Build](https://img.shields.io/github/actions/workflow/status/idealvin/coost/linux.yml?branch=master&logo=linux)](https://github.com/idealvin/coost/actions?query=workflow%3ALinux)
[![Windows Build](https://img.shields.io/github/actions/workflow/status/idealvin/coost/win.yml?branch=master&logo=windows)](https://github.com/idealvin/coost/actions?query=workflow%3AWindows)
[![Mac Build](https://img.shields.io/github/actions/workflow/status/idealvin/coost/macos.yml?branch=master&logo=apple)](https://github.com/idealvin/coost/actions?query=workflow%3AmacOS)
[![Release](https://img.shields.io/github/release/idealvin/coost.svg)](https://github.com/idealvin/coost/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**[A tiny boost library in C++11.](https://github.com/idealvin/coost)**



## 0. Introduction

**[coost](https://github.com/idealvin/coost)** is an elegant and efficient cross-platform C++ base library. Its goal is to create a sword of C++ to make C++ programming easy and enjoyable.

Coost, **co** for short, is like [boost](https://www.boost.org/), but more lightweight, **the static library built on linux or mac is only about 1MB in size**. However, it still provides enough powerful features:

<table>
<tr><td width=33% valign=top>

- Command line and config file parser (flag)
- **High performance log library (log)**
- Unit testing framework
- Bechmark testing framework
- **go-style coroutine**
- Coroutine-based network library
- **JSON RPC framework**

</td><td width=34% valign=top>

- Atomic operation (atomic)
- **Efficient stream (fastream)**
- Efficient string (fastring)
- String utility (str)
- Time library (time)
- Thread library (thread)
- Timed Task Scheduler

</td><td valign=top>

- **God-oriented programming**
- Efficient JSON library
- Hash library
- Path library
- File utilities (fs)
- System operations (os)
- **Fast memory allocator**
 
</td></tr>
</table>




## 1. Sponsor

Coost needs your help. If you are using it or like it, you may consider becoming a sponsor. Thank you very much!

- [Github Sponsors](https://github.com/sponsors/idealvin)
- [A cup of coffee](https://coostdocs.github.io/en/about/sponsor/)




## 2. Documents

- English: [github](https://coostdocs.github.io/en/about/co/) | [gitee](https://coostdocs.gitee.io/en/about/co/)
- 简体中文: [github](https://coostdocs.github.io/cn/about/co/) | [gitee](https://coostdocs.gitee.io/cn/about/co/)




## 3. Core features


### 3.0 God-oriented programming

[co/god.h](https://github.com/idealvin/coost/blob/master/include/co/god.h) provides some features based on templates.

```cpp
#include "co/god.h"

void f() {
    god::bless_no_bugs();
    god::is_same<T, int, bool>(); // T is int or bool?
}
```



### 3.1 flag

**[flag](https://coostdocs.github.io/en/co/flag/)** is a command line and config file parser. It is similar to gflags, but more powerful:
- Support parameters from both command-line and config file.
- Support automatic generation of the config file.
- Support flag aliases.
- Flag of integer type, the value can take a unit `k,m,g,t,p`.

```cpp
#include "co/flag.h"
#include "co/cout.h"

DEF_bool(x, false, "x");
DEF_bool(y, true, "y");
DEF_bool(debug, false, "dbg", d);
DEF_uint32(u, 0, "xxx");
DEF_string(s, "", "xx");

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    cout << "x: " << FLG_x << '\n';
    cout << "y: " << FLG_y << '\n';
    cout << "debug: " << FLG_debug << '\n';
    cout << "u: " << FLG_u << '\n';
    cout << FLG_s << "|" << FLG_s.size() << '\n';
    return 0;
}
```

In the above example, the macros start with `DEF_` define 4 flags. Each flag corresponds to a global variable, whose name is `FLG_` plus the flag name. The flag `debug` has an alias `d`. After building, the above code can run as follow:

```sh
./xx                  # Run with default configs
./xx -x -s good       # x -> true, s -> "good"
./xx -debug           # debug -> true
./xx -xd              # x -> true, debug -> true
./xx -u 8k            # u -> 8192

./xx -mkconf          # Automatically generate a config file: xx.conf
./xx xx.conf          # run with a config file
./xx -conf xx.conf    # Same as above
```



### 3.2 log

**[log](https://coostdocs.github.io/en/co/log/)** is a high-performance log library, some components in coost use it to print logs.

log supports two types of logs: one is level log, which is divided into 5 levels: debug, info, warning, error and fatal, **printing a fatal log will terminate the program**; the other is topic log, logs are grouped by topic, and logs of different topics are written to different files.

```cpp
#include "co/log.h"

int main(int argc, char** argv) {
    flag::parse(argc, argv);

    TLOG("xx") << "s" << 23; // topic log
    DLOG << "hello " << 23;  // debug
    LOG << "hello " << 23;   // info
    WLOG << "hello " << 23;  // warning
    ELOG << "hello " << 23;  // error
    FLOG << "hello " << 23;  // fatal

    return 0;
}
```

co/log also provides a series of `CHECK` macros, which is an enhanced version of `assert`, and they will not be cleared in debug mode.

```cpp
void* p = malloc(32);
CHECK(p != NULL) << "malloc failed..";
CHECK_NE(p, NULL) << "malloc failed..";
```

log is very fast, the following are some test results:

| platform | glog | co/log | speedup |
| ------ | ------ | ------ | ------ |
| win2012 HHD | 1.6MB/s | 180MB/s | 112.5 |
| win10 SSD | 3.7MB/s | 560MB/s | 151.3 |
| mac SSD | 17MB/s | 450MB/s | 26.4 |
| linux SSD | 54MB/s | 1023MB/s | 18.9 |

The above is the write speed of co/log and glog (single thread, 1 million logs). It can be seen that co/log is nearly two orders of magnitude faster than glog.

| threads | linux co/log | linux spdlog | win co/log | win spdlog | speedup |
| ------ | ------ | ------ | ------ | ------ | ------ |
| 1 | 0.087235 | 2.076172 | 0.117704 | 0.461156 | 23.8/3.9 |
| 2 | 0.183160 | 3.729386 | 0.158122 | 0.511769 | 20.3/3.2 |
| 4 | 0.206712 | 4.764238 | 0.316607 | 0.743227 | 23.0/2.3 |
| 8 | 0.302088 | 3.963644 | 0.406025 | 1.417387 | 13.1/3.5 |

The above is the time of [printing 1 million logs with 1, 2, 4, and 8 threads](https://github.com/idealvin/coost/tree/benchmark), in seconds. Speedup is the performance improvement of co/log compared to spdlog on linux and windows platforms.



### 3.3 unitest

[unitest](https://coostdocs.github.io/en/co/unitest/) is a simple and easy-to-use unit test framework. Many components in coost use it to write unit test code, which guarantees the stability of coost.

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
    unitest::run_tests();
    return 0;
}
```

The above is a simple example. The `DEF_test` macro defines a test unit, which is actually a function (a method in a class). The `DEF_case` macro defines test cases, and each test case is actually a code block.

The directory [unitest](https://github.com/idealvin/coost/tree/master/unitest) contains the unit test code in coost. Users can run unitest with the following commands:

```sh
xmake r unitest      # Run all test cases
xmake r unitest -os  # Run test cases in the os unit
```



### 3.4 JSON

In coost v3.0, **[Json](https://github.com/idealvin/coost/blob/master/include/co/json.h)** provides **fluent APIs**, which is more convenient to use.

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

x["a"] == 23;          // true
x["s"] == "123";       // true
x.get("o", "xx") != 0; // false
```

| os | co/json stringify | co/json parse | rapidjson stringify | rapidjson parse | speedup |
| ------ | ------ | ------ | ------ | ------ | ------ |
| win | 569 | 924 | 2089 | 2495 | 3.6/2.7 |
| mac | 783 | 1097 | 1289 | 1658 | 1.6/1.5 |
| linux | 468 | 764 | 1359 | 1070 | 2.9/1.4 |

The above is the average time of stringifying and parsing minimized [twitter.json](https://raw.githubusercontent.com/simdjson/simdjson/master/jsonexamples/twitter.json), in microseconds (us), speedup is the performance improvement of co/json compared to rapidjson.



### 3.5 Coroutine

coost has implemented a [go-style](https://github.com/golang/go) coroutine, which has the following features:

- Support multi-thread scheduling, the default number of threads is the number of system CPU cores.
- Shared stack, coroutines in the same thread share several stacks (the default size is 1MB), and the memory usage is low.
- There is a flat relationship between coroutines, and new coroutines can be created from anywhere (including in coroutines).
- Support coroutine synchronization events, coroutine locks, channels, and waitgroups.

```cpp
#include "co/co.h"

int main(int argc, char** argv) {
    flag::parse(argc, argv);

    co::wait_group wg;
    wg.add(2);

    go([wg](){
        LOG << "hello world";
        wg.done();
    });

    go([wg](){
        LOG << "hello again";
        wg.done();
    });

    wg.wait();
    return 0;
}
```

In the above code, the coroutines created by `go()` will be distributed to different scheduling threads. Users can also control the scheduling of coroutines by themselves:

```cpp
// run f1 and f2 in the same scheduler
auto s = co::next_sched();
s->go(f1);
s->go(f2);

// run f in all schedulers
for (auto& s : co::scheds()) {
    s->go(f);
}
```



### 3.6 network programming

coost provides a coroutine-based network programming framework:

- **[coroutineized socket API](https://coostdocs.github.io/en/co/net/sock/)**, similar in form to the system socket API, users familiar with socket programming can easily write high-performance network programs in a synchronous manner.
- [TCP](https://coostdocs.github.io/en/co/net/tcp/), [HTTP](https://coostdocs.github.io/en/co/net/http/), [RPC](https://coostdocs.github.io/en/co/net/rpc/) and other high-level network programming components, compatible with IPv6, also support SSL, it is more convenient to use than socket API.


**RPC server**

```cpp
#include "co/co.h"
#include "co/rpc.h"
#include "co/time.h"

int main(int argc, char** argv) {
    flag::parse(argc, argv);

    rpc::Server()
        .add_service(new xx::HelloWorldImpl)
        .start("127.0.0.1", 7788, "/xx");

    for (;;) sleep::sec(80000);
    return 0;
}
```

`rpc::Server` also supports HTTP protocol, you may use the POST method to call the RPC service:

```sh
curl http://127.0.0.1:7788/xx --request POST --data '{"api":"ping"}'
```


**Static web server**

```cpp
#include "co/flag.h"
#include "co/http.h"

DEF_string(d, ".", "root dir"); // docroot for the web server

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    so::easy(FLG_d.c_str()); // mum never have to worry again
    return 0;
}
```


**HTTP server**

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


**HTTP client**

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

  Header files of coost.

- [src](https://github.com/idealvin/coost/tree/master/src)

  Source files of coost, built as libco.

- [test](https://github.com/idealvin/coost/tree/master/test)

  Test code, each `.cc` file will be compiled into a separate test program.

- [unitest](https://github.com/idealvin/coost/tree/master/unitest)

  Unit test code, each `.cc` file corresponds to a different test unit, and all code will be compiled into a single test program.

- [gen](https://github.com/idealvin/coost/tree/master/gen)

  A code generator for the RPC framework.




## 5. Building

### 5.1 Compilers required

To build coost, you need a compiler that supports C++11:

- Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
- Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
- Windows: [vs2015+](https://visualstudio.microsoft.com/)


### 5.2 Build with xmake

coost recommends using [xmake](https://github.com/xmake-io/xmake) as the build tool.


#### 5.2.1 Quick start

```sh
# All commands are executed in the root directory of coost (the same below)
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


#### 5.3.5 Find coost in Cmake

```cmake
find_package(coost REQUIRED CONFIG)
target_link_libraries(userTarget coost::co)
```


#### 5.3.6 vcpkg & conan

```sh
vcpkg install coost:x64-windows

# HTTP & SSL support
vcpkg install coost[libcurl,openssl]:x64-windows

conan install coost
```




## 6. License

The MIT license. coost contains codes from some other projects, which have their own licenses, see details in [LICENSE.md](https://github.com/idealvin/coost/blob/master/LICENSE.md).




## 7. Special thanks

- The code of [co/context](https://github.com/idealvin/coost/tree/master/src/co/context) is from [tbox](https://github.com/tboox/tbox) by [ruki](https://github.com/waruqi), special thanks!
- The early English documents of co are translated by [Leedehai](https://github.com/Leedehai) and [daidai21](https://github.com/daidai21), special thanks!
- [ruki](https://github.com/waruqi) has helped to improve the xmake building scripts, thanks in particular!
- [izhengfan](https://github.com/izhengfan) provided cmake building scripts, thank you very much!
- [SpaceIm](https://github.com/SpaceIm) has improved the cmake building scripts, and provided support for `find_package`. Really great help, thank you!
