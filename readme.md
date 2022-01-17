# cocoyaxi

English | [简体中文](readme_cn.md)

[![Linux Build](https://img.shields.io/github/workflow/status/idealvin/cocoyaxi/Linux/master.svg?logo=linux)](https://github.com/idealvin/cocoyaxi/actions?query=workflow%3ALinux)
[![Windows Build](https://img.shields.io/github/workflow/status/idealvin/cocoyaxi/Windows/master.svg?logo=windows)](https://github.com/idealvin/cocoyaxi/actions?query=workflow%3AWindows)
[![Mac Build](https://img.shields.io/github/workflow/status/idealvin/cocoyaxi/macOS/master.svg?logo=apple)](https://github.com/idealvin/cocoyaxi/actions?query=workflow%3AmacOS)
[![Release](https://img.shields.io/github/release/idealvin/cocoyaxi.svg)](https://github.com/idealvin/cocoyaxi/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**A go-style coroutine library in C++11 and more.**



## 0. Introduction

**cocoyaxi (co for short)**, is an elegant and efficient cross-platform C++ base library. It contains a series of high-quality base components, such as **go-style coroutine**, coroutine-based network programming framework, command line and config file parser, high-performance log library, unit testing framework, JSON library, etc.

> It was said that about 23 light-years from the Earth, there is a planet named **Namake**. Namake has three suns, a large one and two small ones. The Namakians make a living by programming. They divide themselves into nine levels according to their programming level, and the three lowest levels will be sent to other planets to develop programming technology. These wandering Namakians must collect at least **10,000 stars** through a project before they can return to Namake.
> 
> Several years ago, two Namakians, [ruki](https://github.com/waruqi) and [alvin](https://github.com/idealvin), were dispatched to the Earth. In order to go back to the Namake planet as soon as possible, ruki has developed a nice build tool [xmake](https://github.com/xmake-io/xmake), whose name is taken from Namake. At the same time, alvin has developed a go-style C++ coroutine library [cocoyaxi](https://github.com/idealvin/cocoyaxi), whose name is taken from the **Cocoyaxi village** where ruki and alvin live on Namake.



## 1. Sponsor

cocoyaxi needs your help. If you are using it or like it, you may consider becoming a sponsor. Thank you very much!

- [Github Sponsors](https://github.com/sponsors/idealvin)
- [A cup of coffee](https://idealvin.github.io/en/about/sponsor/)

**Special Sponsors**

cocoyaxi is specially sponsored by the following companies, thank you very much!

<a href="https://www.oneflow.org/index.html">
<img src="https://idealvin.github.io/images/sponsor/oneflow.png" width="210" height="150">
</a>



## 2. Documents

- English: [github](https://idealvin.github.io/en/about/co/) | [gitee](https://idealvin.gitee.io/en/about/co/)
- 简体中文: [github](https://idealvin.github.io/cn/about/co/) | [gitee](https://idealvin.gitee.io/cn/about/co/)




## 3. Core features


### 3.1 Coroutine

co has implemented a [go-style](https://github.com/golang/go) coroutine, which has the following features:

- Multi-thread scheduling, the default number of threads is the number of system CPU cores.
- Shared stack, coroutines in the same thread share several stacks (the default size is 1MB), and the memory usage is low. Simple test on Linux shows that 10 millions of coroutines only take 2.8G of memory (for reference only).
- There is a flat relationship between coroutines, and new coroutines can be created from anywhere (including in coroutines).
- Support system API hook (Windows/Linux/Mac), you can directly use third-party network library in coroutine.
- Coroutineized [socket API](https://idealvin.github.io/en/co/coroutine/#coroutineized-socket-api).
- Coroutine synchronization event [co::Event](https://idealvin.github.io/en/co/coroutine/#coevent).
- Coroutine lock [co::Mutex](https://idealvin.github.io/en/co/coroutine/#comutex).
- Coroutine pool [co::Pool](https://idealvin.github.io/en/co/coroutine/#copool).
- channel [co::Chan](https://idealvin.github.io/en/co/coroutine/#cochan).
- waitgroup [co::WaitGroup](https://idealvin.github.io/en/co/coroutine/#cowaitgroup).


#### 3.1.1 Create a coroutine

```cpp
go(ku);           // void ku();
go(f, 7);         // void f(int);
go(&T::f, &o);    // void T::f(); T o;
go(&T::f, &o, 7); // void T::f(int); T o;
go([](){
    LOG << "hello go";
});
```

The above is an example of creating coroutines with `go()`. go() is a function that accepts 1 to 3 parameters. The first parameter `f` is any callable object, as long as `f()`, `(*f)()`, `f(p)`, `(*f)(p)`, `(o->*f)()` or `(o->*f)(p)` can be called.

The coroutines created by `go()` will be evenly distributed to different scheduling threads. If you want to create coroutines in **specified scheduling thread**, you can create coroutines in the following way:

```cpp
auto s = co::next_scheduler();
s->go(f1);
s->go(f2);
```

If users want to create coroutine in all scheduling threads, the following way can be used:

```cpp
auto& s = co::all_schedulers();
for (size_t i = 0; i < s.size(); ++i) {
    s[i]->go(f);
}
```


#### 3.1.2 channel

[co::Chan](https://idealvin.github.io/en/co/coroutine/#cochan), similar to the channel in golang, can be used to transfer data between coroutines.

```cpp
#include "co/co.h"

DEF_main(argc, argv) {
    co::Chan<int> ch;
    go([ch]() { /* capture by value, rather than reference */
        ch << 7;
    });

    int v = 0;
    ch >> v;
    LOG << "v: " << v;

    return 0;
}
```

When creating a channel, we can add a timeout as follows:

```cpp
co::Chan<int> ch(8, 1000);
```

After read or write operation, we can call `co::timeout()` to determine whether it has timed out. This method is simpler than the select-based implementation in golang. For detailed usage, see [Document of co::Chan](https://idealvin.github.io/en/co/coroutine/#cochan).


#### 3.1.3 waitgroup

[co::WaitGroup](https://idealvin.github.io/en/co/coroutine/#cowaitgroup), similar to `sync.WaitGroup` in golang, can be used to wait for the exit of coroutines or threads.

```cpp
#include "co/co.h"

DEF_main(argc, argv) {
    FLG_cout = true;

    co::WaitGroup wg;
    wg.add(8);

    for (int i = 0; i < 8; ++i) {
        go([wg]() {
            LOG << "co: " << co::coroutine_id();
            wg.done();
        });
    }

    wg.wait();
    return 0;
}
```



### 3.2 network programming

co provides a set of coroutineized [socket APIs](https://idealvin.github.io/en/co/coroutine/#coroutineized-socket-api), most of them are consistent with the native socket APIs in form, with which, you can easily write high-performance network programs in a synchronous manner.

In addition, co has also implemented higher-level network programming components, including [TCP](https://idealvin.github.io/en/co/net/tcp/), [HTTP](https://idealvin.github.io/en/co/net/http/) and [RPC](https://idealvin.github.io/en/co/net/rpc/) framework based on [JSON](https://idealvin.github.io/en/co/json/), they are IPv6-compatible and support SSL at the same time, which is more convenient than socket APIs. Here is just a brief demonstration of the usage of HTTP, and the rest can be seen in the documents.


#### 3.2.1 Static web server

```cpp
#include "co/flag.h"
#include "co/log.h"
#include "co/so.h"

DEF_string(d, ".", "root dir"); // Specify the root directory of the web server

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    so::easy(FLG_d.c_str()); // mum never have to worry again

    return 0;
}
```


#### 3.2.2 HTTP server

```cpp
http::Server serv;

serv.on_req(
    [](const http::Req& req, http::Res& res) {
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
);

serv.start("0.0.0.0", 80);                                    // http
serv.start("0.0.0.0", 443, "privkey.pem", "certificate.pem"); // https
```


#### 3.2.3 HTTP client

```cpp
void f() {
    http::Client c("https://github.com");

    c.get("/");
    LOG << "response code: "<< c.response_code();
    LOG << "body size: "<< c.body_size();
    LOG << "Content-Length: "<< c.header("Content-Length");
    LOG << c.header();

    c.post("/hello", "data xxx");
    LOG << "response code: "<< c.response_code();
}

go(f);
```



### 3.3 co/flag

[co/flag](https://idealvin.github.io/en/co/flag/) is a command line and config file parser similar to [google gflags](https://github.com/gflags/gflags), but more simple and easier to use. Some components in co use it to define config items.

co/flag provides a default value for each config item. Without config parameters, the program can run with the default config. Users can also pass in config parameters from **command line or config file**. When a config file is needed, users can run `./exe -mkconf` to **generate a config file**.

```cpp
// xx.cc
#include "co/flag.h"
#include "co/cout.h"

DEF_bool(x, false, "bool x");
DEF_bool(y, false, "bool y");
DEF_uint32(u32, 0, "...");
DEF_string(s, "hello world", "string");

int main(int argc, char** argv) {
    flag::init(argc, argv);

    COUT << "x: "<< FLG_x;
    COUT << "y: "<< FLG_y;
    COUT << "u32: "<< FLG_u32;
    COUT << FLG_s << "|" << FLG_s.size();

    return 0;
}
```

The above is an example of using co/flag. The macro at the beginning of `DEF_` in the code defines 4 config items. Each config item is equivalent to a global variable. The variable name is `FLG_` plus the config name. After the above code is compiled, it can be run as follows:

```sh
./xx                  # Run with default configs
./xx -xy -s good      # single letter named bool flags, can be set to true together
./xx -s "I'm ok"      # string with spaces
./xx -u32 8k          # Integers can have units: k,m,g,t,p, not case sensitive

./xx -mkconf          # Automatically generate a config file: xx.conf
./xx xx.conf          # run with a config file
./xx -config xx.conf  # Same as above
```



### 3.4 co/log

[co/log](https://idealvin.github.io/en/co/log/) is a high-performance and memory-friendly local log library, which nearly needs no memory allocation. Some components in co will use it to print logs.

co/log divides the log into five levels: debug, info, warning, error, and fatal. **Printing a fatal level log will terminate the program**. Users can print logs of different levels as follows:

```cpp
DLOG << "hello " << 23; // debug
LOG << "hello " << 23;  // info
WLOG << "hello " << 23; // warning
ELOG << "hello " << 23; // error
FLOG << "hello " << 23; // fatal
```

co/log also provides a series of `CHECK` macros, which can be regarded as an enhanced version of `assert`, and they will not be cleared in debug mode.

```cpp
void* p = malloc(32);
CHECK(p != NULL) << "malloc failed..";
CHECK_NE(p, NULL) << "malloc failed..";
```

When the CHECK assertion failed, co/log will print the function call stack information, and then terminate the program. On linux and macosx, make sure you have installed [libbacktrace](https://github.com/ianlancetaylor/libbacktrace) on your system.

![stack](https://idealvin.github.io/images/stack.png)

co/log is very fast. The following are some test results, for reference only:

- co/log vs glog (single thread)

  | platform | google glog | co/log |
  | ------ | ------ | ------ |
  | win2012 HHD | 1.6MB/s | 180MB/s |
  | win10 SSD | 3.7MB/s | 560MB/s |
  | mac SSD | 17MB/s | 450MB/s |
  | linux SSD | 54MB/s | 1023MB/s |

- [co/log vs spdlog](https://github.com/idealvin/co/tree/benchmark) (Windows)

  | threads | total logs | co/log time(seconds) | spdlog time(seconds)|
  | ------ | ------ | ------ | ------ |
  | 1 | 1000000 | 0.103619 | 0.482525 |
  | 2 | 1000000 | 0.202246 | 0.565262 |
  | 4 | 1000000 | 0.330694 | 0.722709 |
  | 8 | 1000000 | 0.386760 | 1.322471 |

- [co/log vs spdlog](https://github.com/idealvin/co/tree/benchmark) (Linux)

  | threads | total logs | co/log time(seconds) | spdlog time(seconds)|
  | ------ | ------ | ------ | ------ |
  | 1 | 1000000 | 0.096445 | 2.006087 |
  | 2 | 1000000 | 0.142160 | 3.276006 |
  | 4 | 1000000 | 0.181407 | 4.339714 |
  | 8 | 1000000 | 0.303968 | 4.700860 |



### 3.5 co/unitest

[co/unitest](https://idealvin.github.io/en/co/unitest/) is a simple and easy-to-use unit test framework. Many components in co use it to write unit test code, which guarantees the stability of co.

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

The above is a simple example. The `DEF_test` macro defines a test unit, which is actually a function (a method in a class). The `DEF_case` macro defines test cases, and each test case is actually a code block. Multiple test units can be put in the same C++ project, the main function is simple as below:

```cpp
#include "co/unitest.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    unitest::run_all_tests();
    return 0;
}
```

[unitest](https://github.com/idealvin/cocoyaxi/tree/master/unitest) contains the unit test code in cocoyaxi. Users can run unitest with the following commands:

```sh
xmake r unitest -a   # Run all test cases
xmake r unitest -os  # Run test cases in the os unit
```




## 4. Code composition

- [include](https://github.com/idealvin/cocoyaxi/tree/master/include)

  Header files of co.

- [src](https://github.com/idealvin/cocoyaxi/tree/master/src)

  Source files of co, built as libco.

- [test](https://github.com/idealvin/cocoyaxi/tree/master/test)

  Some test code, each `.cc` file will be compiled into a separate test program.

- [unitest](https://github.com/idealvin/cocoyaxi/tree/master/unitest)

  Some unit test code, each `.cc` file corresponds to a different test unit, and all code will be compiled into a single test program.

- [gen](https://github.com/idealvin/cocoyaxi/tree/master/gen)

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

#### 5.2.3 Build with mingw on Windows

```sh
xmake f -p mingw
xmake -v
```


#### 5.2.4 Enable HTTP & SSL features

```sh
xmake f --with_libcurl=true --with_openssl=true
xmake -a
```


#### 5.2.5 Install libco

```sh
# Install header files and libco by default.
xmake install -o pkg         # package related files to the pkg directory
xmake i -o pkg               # the same as above
xmake install -o /usr/local  # install to the /usr/local directory
```


#### 5.2.6 Install libco from xmake repo

```sh
xrepo install -f "openssl=true,libcurl=true" cocoyaxi
```



### 5.3 Build with cmake

[izhengfan](https://github.com/izhengfan) helped to provide cmake support:


#### 5.3.1 Build libco by default

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


#### 5.3.3 Enable HTTP & SSL features

```sh
mkdir build && cd build
cmake .. -DBUILD_ALL=ON -DCMAKE_INSTALL_PREFIX=/usr/local
make -j8
make install
```


#### 5.3.4 Build shared library

```sh
cmake .. -DBUILD_SHARED_LIBS=ON
```


#### 5.3.5 Install libco from vcpkg

```sh
vcpkg install cocoyaxi:x64-windows

# HTTP & SSL support
vcpkg install cocoyaxi[libcurl,openssl]:x64-windows
```

#### 5.3.6 Install libco from conan

```sh
conan install cocoyaxi
```



## 6. License

The MIT license. cocoyaxi contains codes from some other projects, which have their own licenses, see details in [LICENSE.md](https://github.com/idealvin/cocoyaxi/blob/master/LICENSE.md).




## 7. Special thanks

- The code of [co/context](https://github.com/idealvin/cocoyaxi/tree/master/src/co/context) is from [tbox](https://github.com/tboox/tbox) by [ruki](https://github.com/waruqi), special thanks!
- The early English documents of co are translated by [Leedehai](https://github.com/Leedehai) and [daidai21](https://github.com/daidai21), special thanks!
- [ruki](https://github.com/waruqi) has helped to improve the xmake building scripts, thanks in particular!
- [izhengfan](https://github.com/izhengfan) provided cmake building scripts, thank you very much!
