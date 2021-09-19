<div align="center">
  <div>
    <a href="https://github.com/idealvin/co/blob/master/LICENSE.md">
      <img src="https://img.shields.io/badge/license-MIT-brightgreen?style=flat-square" alt="license" />
    </a>
    <a href="https://github.com/idealvin/co/releases">
      <img src="https://img.shields.io/github/release/idealvin/co.svg?style=flat-square" alt="Github All Releases" />
    </a>
  </div>
  <div>
    <a href="https://github.com/idealvin/co/actions?query=workflow%3AWindows">
      <img src="https://img.shields.io/github/workflow/status/idealvin/co/Windows/master.svg?style=flat-square&logo=windows" alt="github-ci" />
    </a>
    <a href="https://github.com/idealvin/co/actions?query=workflow%3ALinux">
      <img src="https://img.shields.io/github/workflow/status/idealvin/co/Linux/master.svg?style=flat-square&logo=linux" alt="github-ci" />
    </a>
    <a href="https://github.com/idealvin/co/actions?query=workflow%3AmacOS">
      <img src="https://img.shields.io/github/workflow/status/idealvin/co/macOS/master.svg?style=flat-square&logo=apple" alt="github-ci" />
    </a>
    <a href="https://github.com/idealvin/co/actions?query=workflow%3AAndroid">
      <img src="https://img.shields.io/github/workflow/status/idealvin/co/Android/master.svg?style=flat-square&logo=android" alt="github-ci" />
    </a>
  </div>

  <div>
  <span style="border-bottom: 3px double #ccc">A go-style C++ coroutine library and more.</span>
  </div>
</div>


## Basic [(中文)](readme_cn.md)

CO is an elegant and efficient C++ base library that supports Linux, Windows and Mac platforms. It implements a series of high-quality base components, such as go-style coroutine, coroutine-based network programming framework, command line parameter and config file parsing library, high-performance log library, unit testing framework, JSON library, etc.




## Sponsor

Goto the [Sponsor](https://idealvin.github.io/en/about/sponsor/) page.




## Documents

- English: [github](https://idealvin.github.io/en/about/co/) [gitee](https://idealvin.gitee.io/en/about/co/)
- 中文: [github](https://idealvin.github.io/cn/about/co/) [gitee](https://idealvin.gitee.io/cn/about/co/)




## Core features


### Coroutine

CO has implemented a [go-style](https://github.com/golang/go) coroutine, which has the following features:

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


#### Create a coroutine

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

The coroutines created by `go()` will be evenly distributed to different scheduling threads. If users want some coroutines to run in the same thread, they can create coroutines in the following way:

```cpp
auto s = co::next_scheduler();
s->go(f1);
s->go(f2);
```

If users want to create coroutine in all scheduling threads, the following way can be used:

```cpp
auto& s = co::all_schedulers();
for (size_t i = 0; i <s.size(); ++i) {
    s[i]->go(f);
}
```


#### channel

[co::Chan](https://idealvin.github.io/en/co/coroutine/#cochan), similar to the channel in golang, can be used to transfer data between coroutines.

```cpp
#include "co/co.h"

DEF_main(argc, argv) {
    co::Chan<int> ch;
    go([ch]() {
        ch << 7;
    });

    int v = 0;
    ch >> v;
    LOG << "v: "<< v;

    return 0;
}
```

**The read and write operations of channel must be performed in coroutine**, so the main function is defined with `DEF_main` in above code, so that code in the main function also runs in coroutine.

The channel object in the code is on the stack, while CO uses a shared stack implementation. Data on the stack of one coroutine may be overwritten by other coroutines. Generally, we cann't share data on stack between coroutines. Therefore, we **capture by value** in the lambda to copy the channel and pass it to the newly created coroutine. The copy operation just increases the internal reference count by 1 and will not affect the performance.

When creating a channel, we can add a timeout as follows:

```cpp
co::Chan<int> ch(8, 1000);
```

After read or write operation, we can call `co::timeout()` to determine whether it has timed out. This method is simpler than the select-based implementation in golang.

The channel in CO is implemented based on memory copy. The data type passed can be a built-in type, a pointer type, or a structure type with simple memory copy semantics for the copy operation. Like `std::string` or container types in STL, the copy operation is not a simple memory copy, and generally cannot be passed directly in channel. For details, see [Document of co::Chan](https://idealvin.github.io/en/co/coroutine/#cochan).


#### waitgroup

[co::WaitGroup](https://idealvin.github.io/en/co/coroutine/#cowaitgroup), similar to `sync.WaitGroup` in golang, can be used to wait for the exit of coroutines or threads.

```cpp
#include "co/co.h"

DEF_main(argc, argv) {
    FLG_cout = true;

    co::WaitGroup wg;
    wg.add(8);

    for (int i = 0; i <8; ++i) {
        go([wg]() {
            LOG << "co: "<< co::coroutine_id();
            wg.done();
        });
    }

    wg.wait();
    return 0;
}
```



### network programming

CO provides a set of coroutineized [socket APIs](https://idealvin.github.io/en/co/coroutine/#coroutineized-socket-api), most of them are consistent with the native socket APIs in form. In addition, CO has also implemented higher-level network programming components, including [TCP](https://idealvin.github.io/en/co/net/tcp/), [HTTP](https://idealvin.github.io/en/co/net/http/) and [RPC](https://idealvin.github.io/en/co/net/rpc/) framework based on [JSON](https://idealvin.github.io/en/co/json/), they are IPv6-compatible and support SSL at the same time, which is more convenient than socket APIs. Here is just a brief demonstration of the usage of HTTP, and the rest can be viewed in the documents.


**Static web server**

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


**HTTP server**

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


**HTTP client**

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



### co/flag

[co/flag](https://idealvin.github.io/en/co/flag/) is a simple and easy-to-use command line and config file parsing library. Some components in CO use it to define config items.

co/flag provides a default value for each config item. Without config parameters, the program can run with the default config. Users can also pass in config parameters from **command line or config file**. When a config file is needed, users can run `./exe -mkconf` to **generate a config file**.

```cpp
// xx.cc
#include "co/flag.h"
#include "co/log.h"

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



### co/log

[co/log](https://idealvin.github.io/en/co/log/) is a high-performance local log library, some components in CO use it to print logs.

co/log divides the log into five levels: debug, info, warning, error, and fatal. **Printing a fatal level log will terminate the program**. Users can print logs of different levels as follows:

```cpp
DLOG << "hello "<< 23; // debug
LOG << "hello "<< 23;  // info
WLOG << "hello "<< 23; // warning
ELOG << "hello "<< 23; // error
FLOG << "hello "<< 23; // fatal
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



### co/unitest

[co/unitest](https://idealvin.github.io/en/co/unitest/) is a simple and easy-to-use unit test framework. Many components in CO use it to write unit test code, which guarantees the stability of CO.

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

[co/unitest](https://github.com/idealvin/co/tree/master/unitest) contains the unit test code in CO. Users can run unitest with the following commands:

```sh
xmake r unitest -a   # Run all test cases
xmake r unitest -os  # Run test cases in the os unit
```




## Code composition

- [co/include](https://github.com/idealvin/co/tree/master/include)

  Header files of `libco`.

- [co/src](https://github.com/idealvin/co/tree/master/src)

  Source files of `libco`.

- [co/test](https://github.com/idealvin/co/tree/master/test)

  Some test code, each `.cc` file will be compiled into a separate test program.

- [co/unitest](https://github.com/idealvin/co/tree/master/unitest)

  Some unit test code, each `.cc` file corresponds to a different test unit, and all code will be compiled into a single test program.

- [co/gen](https://github.com/idealvin/co/tree/master/gen)

  A code generator for the RPC framework.




## Building

### Compilers required

- Compiler
  - Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
  - Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
  - Windows: [vs2015+](https://visualstudio.microsoft.com/)


### xmake

co recommends using [xmake](https://github.com/xmake-io/xmake) as the build tool.


#### Quick start

```sh
# All commands are executed in the root directory of co (the same below)
xmake      # build libco by default
xmake -a   # build all projects (libco, gen, co/test, co/unitest)
```


#### Build with mingw

```sh
xmake f -p mingw
xmake -v
```


#### Enable HTTP & SSL features

```sh
xmake f --with_libcurl=true --with_openssl=true
xmake -a
```


#### Install libco

```sh
# Install header files and libco by default.
xmake install -o pkg         # package related files to the pkg directory
xmake i -o pkg               # the same as above
xmake install -o /usr/local  # install to the /usr/local directory
```


#### Install libco from xmake repo

```sh
xrepo install -f "openssl=true,libcurl=true" co
```



### cmake

[izhengfan](https://github.com/izhengfan) helped to provide cmake support:


#### Build libco by default

```sh
mkdir build && cd build
cmake ..
make -j8
```


#### Build all projects

```sh
mkdir build && cd build
cmake .. -DBUILD_ALL=ON
make -j8
```


#### Build with libcurl & openssl (openssl 1.1.0 or above required)

```sh
mkdir build && cd build
cmake .. -DBUILD_ALL=ON -DCMAKE_INSTALL_PREFIX=/usr/local
make -j8
make install
```


#### install libco from vcpkg

```sh
vcpkg install co:x64-windows

# HTTP & SSL support
vcpkg install co[libcurl,openssl]:x64-windows
```




## License

The MIT license. co contains codes from some other projects, which have their own licenses, see details in [LICENSE.md](https://github.com/idealvin/co/blob/master/LICENSE.md).




## Special thanks

- The code of [co/context](https://github.com/idealvin/co/tree/master/src/co/context) is from [tbox](https://github.com/tboox/tbox) by [ruki](https://github.com/waruqi), special thanks!
- The English reference documents of co are translated by [Leedehai](https://github.com/Leedehai) (1-10), [daidai21](https://github.com/daidai21) (11-15) and [google](https://translate.google.cn/), special thanks!
- [ruki](https://github.com/waruqi) has helped to improve the xmake building scripts, thanks in particular!
- [izhengfan](https://github.com/izhengfan) provided cmake building scripts, thank you very much!
