# coost

[English](readme.md) | 简体中文
[![Linux Build](https://img.shields.io/github/actions/workflow/status/idealvin/coost/linux.yml?branch=master&logo=linux)](https://github.com/idealvin/coost/actions?query=workflow%3ALinux)
[![Windows Build](https://img.shields.io/github/actions/workflow/status/idealvin/coost/win.yml?branch=master&logo=windows)](https://github.com/idealvin/coost/actions?query=workflow%3AWindows)
[![Mac Build](https://img.shields.io/github/actions/workflow/status/idealvin/coost/macos.yml?branch=master&logo=apple)](https://github.com/idealvin/coost/actions?query=workflow%3AmacOS)
[![Release](https://img.shields.io/github/release/idealvin/coost.svg)](https://github.com/idealvin/coost/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)


**[A tiny boost library in C++11.](https://github.com/idealvin/coost)**



## 0. coost 简介

**[coost](https://github.com/idealvin/coost)** 是一个**兼具性能与易用性**的跨平台 C++ 基础库，其目标是打造一把 C++ 开发神器，让 C++ 编程变得简单、轻松、愉快。

coost 简称为 co，曾被称为小型 [boost](https://www.boost.org/) 库，与 boost 相比，coost 小而精美，在 **linux 与 mac 上编译出来的静态库仅 1M 左右大小**，却包含了不少强大的功能：

<table>
<tr><td width=33% valign=top>

- 命令行与配置文件解析(flag)
- **高性能日志库(log)**
- 单元测试框架
- 基准测试框架
- **go-style 协程**
- 基于协程的网络编程框架
- **基于 JSON 的 RPC 框架**

</td><td width=34% valign=top>

- 原子操作(atomic)
- **高效字符流(fastream)**
- 高效字符串(fastring)
- 字符串操作(str)
- 时间库(time)
- 线程库(thread)
- 定时任务调度器

</td><td valign=top>

- **面向玄学编程**
- 高效 JSON 库
- hash 库
- path 库
- 文件系统操作(fs)
- 系统操作(os)
- **高性能内存分配器**

</td></tr>
</table>


## 1. 赞助

coost 的发展离不开大家的帮助，如果您在使用或者喜欢 coost，可以考虑赞助本项目，非常感谢。

- [Github Sponsors](https://github.com/sponsors/idealvin)
- [给作者来杯咖啡](https://coostdocs.gitee.io/cn/about/sponsor/)




## 2. 参考文档

- 简体中文: [github](https://coostdocs.github.io/cn/about/co/) | [gitee](https://coostdocs.gitee.io/cn/about/co/)
- English: [github](https://coostdocs.github.io/en/about/co/) | [gitee](https://coostdocs.gitee.io/en/about/co/)




## 3. 核心组件


### 3.0 面向玄学编程

[co/god.h](https://github.com/idealvin/coost/blob/master/include/co/god.h) 提供模板相关的一些功能。模板用到深处有点玄，有些 C++ 程序员称之为面向玄学编程。

```cpp
#include "co/god.h"

void f() {
    god::bless_no_bugs();
    god::is_same<T, int, bool>(); // T is int or bool?
}
```



### 3.1 flag

**[flag](https://coostdocs.github.io/cn/co/flag/)** 是一个命令行参数与配置文件解析库，用法与 gflags 类似，但功能更加强大：
- 支持从命令行、配置文件传入参数。
- 支持自动生成配置文件。
- 支持 flag 别名。
- 整数类型的 flag，值可以带单位 `k,m,g,t,p`，不分大小写。

```cpp
// xx.cc
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

上述代码中 `DEF_` 开头的宏定义了 4 个 flag，每个 flag 对应一个全局变量，变量名是 `FLG_` 加 flag 名，其中 flag `debug` 还有一个别名 `d`。上述代码编译后，可以按如下方式运行：

```sh
./xx                  # 按默认配置运行
./xx -x -s good       # x -> true, s -> "good"
./xx -debug           # debug -> true
./xx -xd              # x -> true, debug -> true
./xx -u 8k            # u -> 8192, 整数可带单位(k,m,g,t,p), 不分大小写

./xx -mkconf          # 自动生成配置文件 xx.conf
./xx xx.conf          # 从配置文件传入参数
./xx -conf xx.conf    # 与上同
```



### 3.2 log

**[log](https://coostdocs.github.io/cn/co/log/)** 是一个高性能日志组件，coost 中的一些组件用它打印日志。

log 支持两种类型的日志：一种是 level log，分为 debug, info, warning, error, fatal 5 个级别，**打印 fatal 级别的日志会终止程序的运行**；另一种是 topic log，日志按 topic 分类，不同 topic 的日志写入不同的文件。

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

log 还提供了一系列 `CHECK` 宏，可以视为加强版的 `assert`，它们在 debug 模式下也不会被清除。CHECK 断言失败时，log 会打印函数调用栈信息，然后终止程序的运行。

```cpp
void* p = malloc(32);
CHECK(p != NULL) << "malloc failed..";
CHECK_NE(p, NULL) << "malloc failed..";
```

log 速度非常快，下面是一些测试结果：

| platform | glog | co/log | speedup |
| ------ | ------ | ------ | ------ |
| win2012 HHD | 1.6MB/s | 180MB/s | 112.5 |
| win10 SSD | 3.7MB/s | 560MB/s | 151.3 |
| mac SSD | 17MB/s | 450MB/s | 26.4 |
| linux SSD | 54MB/s | 1023MB/s | 18.9 |

上表是 co/log 与 glog 在单线程连续打印 100 万条日志时测得的写速度对比，可以看到 co/log 比 glog 快了近两个数量级。

| threads | linux co/log | linux spdlog | win co/log | win spdlog | speedup |
| ------ | ------ | ------ | ------ | ------ | ------ |
| 1 | 0.087235 | 2.076172 | 0.117704 | 0.461156 | 23.8/3.9 |
| 2 | 0.183160 | 3.729386 | 0.158122 | 0.511769 | 20.3/3.2 |
| 4 | 0.206712 | 4.764238 | 0.316607 | 0.743227 | 23.0/2.3 |
| 8 | 0.302088 | 3.963644 | 0.406025 | 1.417387 | 13.1/3.5 |

上表是分别[用 1、2、4、8 个线程打印 100 万条日志](https://github.com/idealvin/coost/tree/benchmark)的耗时，单位为秒，speedup 是 co/log 在 linux、windows 平台相对于 spdlog 的性能提升倍数。



### 3.3 unitest

**[unitest](https://coostdocs.github.io/cn/co/unitest/)** 是一个简单易用的单元测试框架，coost 中的很多组件用它写单元测试代码，为 coost 的稳定性提供了重要保障。

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

上面是一个简单的例子，`DEF_test` 宏定义了一个测试单元，实际上就是一个函数(类中的方法)。`DEF_case` 宏定义了测试用例，每个测试用例实际上就是一个代码块。

[unitest](https://github.com/idealvin/coost/tree/master/unitest) 目录下面是 coost 中的单元测试代码，编译后可执行下述命令运行：

```sh
xmake r unitest      # 运行所有单元测试用例
xmake r unitest -os  # 仅运行 os 单元中的测试用例
```



### 3.4 JSON

coost v3.0 中，**[Json](https://github.com/idealvin/coost/blob/master/include/co/json.h)** 采用**流畅(fluent)接口设计**，用起来更加方便。

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

下面是 co/json 与 rapidjson 的性能对比：

| os | co/json stringify | co/json parse | rapidjson stringify | rapidjson parse | speedup |
| ------ | ------ | ------ | ------ | ------ | ------ |
| win | 569 | 924 | 2089 | 2495 | 3.6/2.7 |
| mac | 783 | 1097 | 1289 | 1658 | 1.6/1.5 |
| linux | 468 | 764 | 1359 | 1070 | 2.9/1.4 |

上表是将 [twitter.json](https://raw.githubusercontent.com/simdjson/simdjson/master/jsonexamples/twitter.json) 最小化后测得的 stringify 及 parse 的平均耗时，单位为微秒(us)，speedup 是 co/json 在 stringify、parse 方面相对于 rapidjson 的性能提升倍数。



### 3.5 协程

coost 实现了类似 golang 中 goroutine 的协程机制，它有如下特性：

- 支持多线程调度，默认线程数为系统 CPU 核数。
- 共享栈，同一线程中的协程共用若干个栈(大小默认为 1MB)，内存占用低。
- 各协程之间为平级关系，可以在任何地方(包括在协程中)创建新的协程。
- 支持协程同步事件、协程锁、channel、waitgroup 等协程同步机制。

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

上面的代码中，`go()` 创建的协程会分配到不同的调度线程中。用户也可以自行控制协程的调度：

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



### 3.6 网络编程

coost 提供了一套基于协程的网络编程框架:

- **[协程化的 socket API](https://coostdocs.github.io/cn/co/net/sock/)**，形式上与系统 socket API 类似，熟悉 socket 编程的用户，可以轻松的用同步的方式写出高性能的网络程序。
- [TCP](https://coostdocs.github.io/cn/co/net/tcp/)、[HTTP](https://coostdocs.github.io/cn/co/net/http/)、[RPC](https://coostdocs.github.io/cn/co/net/rpc/) 等高层网络编程组件，兼容 IPv6，同时支持 SSL，用起来比 socket API 更方便。


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

rpc::Server 同时支持 HTTP 协议，可以用 HTTP 的 POST 方法调用 RPC 服务：

```sh
curl http://127.0.0.1:7788/xx --request POST --data '{"api":"ping"}'
```


**静态 web server**

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




## 4. 代码构成

- [include](https://github.com/idealvin/coost/tree/master/include)  

  coost 的头文件。

- [src](https://github.com/idealvin/coost/tree/master/src)  

  coost 的源代码，编译生成 libco。

- [test](https://github.com/idealvin/coost/tree/master/test)  

  测试代码，每个 `.cc` 文件都会编译成一个单独的测试程序。

- [unitest](https://github.com/idealvin/coost/tree/master/unitest)  

  单元测试代码，每个 `.cc` 文件对应不同的测试单元，所有代码都会编译到单个测试程序中。

- [gen](https://github.com/idealvin/coost/tree/master/gen)  

  代码生成工具。




## 5. 构建

### 5.1 编译器要求

编译 coost 需要编译器支持 C++11：

- Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
- Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
- Windows: [vs2015+](https://visualstudio.microsoft.com/)


### 5.2 用 xmake 构建

coost 推荐使用 [xmake](https://github.com/xmake-io/xmake) 作为构建工具。


#### 5.2.1 快速上手

```sh
# 所有命令都在 coost 根目录执行，后面不再说明
xmake       # 默认构建 libco
xmake -a    # 构建所有项目 (libco, gen, test, unitest)
```


#### 5.2.2 构建动态库

```sh
xmake f -k shared
xmake -v
```


#### 5.2.3 基于 mingw 构建

```sh
xmake f -p mingw
xmake -v
```


#### 5.2.4 启用 HTTP/SSL 特性

```sh
xmake f --with_libcurl=true --with_openssl=true
xmake -v
```


#### 5.2.5 安装 libco

```sh
xmake install -o pkg          # 打包安装到 pkg 目录
xmake i -o pkg                # 同上
xmake install -o /usr/local   # 安装到 /usr/local 目录
```


#### 5.2.6 从 xrepo 安装 libco

```sh
xrepo install -f "openssl=true,libcurl=true" coost
```



### 5.3 用 cmake 构建

[izhengfan](https://github.com/izhengfan) 帮忙提供了 cmake 支持，[SpaceIm](https://github.com/SpaceIm) 进一步完善了 cmake 脚本。


#### 5.3.1 构建 libco

```sh
mkdir build && cd build
cmake ..
make -j8
```


#### 5.3.2 构建所有项目

```sh
mkdir build && cd build
cmake .. -DBUILD_ALL=ON -DCMAKE_INSTALL_PREFIX=/usr/local
make -j8
make install
```


#### 5.3.3 启用 HTTP/SSL 特性

```sh
mkdir build && cd build
cmake .. -DWITH_LIBCURL=ON -DWITH_OPENSSL=ON
make -j8
```


#### 5.3.4 构建动态库

```sh
cmake .. -DBUILD_SHARED_LIBS=ON
make -j8
```


#### 5.3.5 cmake 中查找 coost 包

```cmake
find_package(coost REQUIRED CONFIG)
target_link_libraries(userTarget coost::co)
```


#### 5.3.6 vcpkg & conan

```sh
vcpkg install coost:x64-windows

# 启用 HTTP & SSL
vcpkg install coost[libcurl,openssl]:x64-windows

conan install coost
```




## 6. License

The MIT license. coost 包含了一些其他项目的代码，可能使用了不同的 License，详情见 [LICENSE.md](https://github.com/idealvin/coost/blob/master/LICENSE.md)。




## 7. 特别致谢

- [context](https://github.com/idealvin/coost/tree/master/src/co/context) 的相关代码取自 [ruki](https://github.com/waruqi) 的 [tbox](https://github.com/tboox/tbox)，特别表示感谢！
- [Leedehai](https://github.com/Leedehai) 与 [daidai21](https://github.com/daidai21) 早期帮忙将 coost 的中文参考文档翻译成英文，特别表示感谢！
- [ruki](https://github.com/waruqi) 帮忙改进了 xmake 构建脚本，特别表示感谢！
- [izhengfan](https://github.com/izhengfan) 提供了 cmake 构建脚本，特别表示感谢！
- [SpaceIm](https://github.com/SpaceIm) 完善了 cmake 构建脚本，提供了 `find_package` 的支持，特别表示感谢！

