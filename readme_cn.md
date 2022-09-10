# Coost

[English](readme.md) | 简体中文

[![Linux Build](https://img.shields.io/github/workflow/status/idealvin/coost/Linux/master.svg?logo=linux)](https://github.com/idealvin/coost/actions?query=workflow%3ALinux)
[![Windows Build](https://img.shields.io/github/workflow/status/idealvin/coost/Windows/master.svg?logo=windows)](https://github.com/idealvin/coost/actions?query=workflow%3AWindows)
[![Mac Build](https://img.shields.io/github/workflow/status/idealvin/coost/macOS/master.svg?logo=apple)](https://github.com/idealvin/coost/actions?query=workflow%3AmacOS)
[![Release](https://img.shields.io/github/release/idealvin/coost.svg)](https://github.com/idealvin/coost/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)


**A tiny boost library in C++11.**



## 0. Coost 简介

**Coost** 是一个优雅、高效的跨平台 C++ 基础库，它没有 [boost](https://www.boost.org/) 那么重，但仍然提供了足够强大的功能：

- 命令行参数与配置文件解析库(flag)
- 高性能日志库(log)
- 单元测试框架(unitest)
- go-style 协程
- 基于协程的网络编程框架
- 高效 JSON 库
- 基于 JSON 的 RPC 框架
- 面向玄学编程
- 原子操作(atomic)
- 随机数生成器(random)
- 高效字符流(fastream)
- 高效字符串(fastring)
- 字符串操作(str)
- 时间库(time)
- 线程库(thread)
- 定时任务调度器
- 高效内存分配器
- LruMap
- hash 库
- path 库
- 文件系统操作(fs)
- 系统操作(os)
 
Coost 原名 cocoyaxi(简称co)，因担心暴露过多信息而使 Namake 星遭受**黑暗森林**打击，故改名为 Coost，意为比 boost 更加轻量级的 C++ 基础库。

> 传说在距离地球约xx光年的地方，有一颗名为**娜美克**(**Namake**)的行星，娜美克星有一大两小三个太阳。娜美克星人以编程为生，他们按编程水平将所有人分成九个等级，水平最低的三个等级会被送往其他星球发展编程技术。这些外派的娜美克星人，必须通过一个项目，**集齐至少10000个赞**，才能重返娜美克星。
> 
> 若干年前，两个娜美克星人 [ruki](https://github.com/waruqi) 和 [alvin](https://github.com/idealvin)，被发配到地球上。为了早日回到娜美克星，ruki 开发了一个强大的构建工具 [xmake](https://github.com/xmake-io/xmake)，该名字就是取自 Namake。alvin 则开发了一个微型 boost 库 [coost](https://github.com/idealvin/coost), 其原名 cocoyaxi 取自 ruki 和 alvin 在娜美克星上居住的可可亚西村。



## 1. 赞助

Coost 的发展离不开大家的帮助，如果您在使用或者喜欢 Coost，可以考虑赞助本项目，非常感谢🙏

- [Github Sponsors](https://github.com/sponsors/idealvin)
- [给作者来杯咖啡](https://coostdocs.gitee.io/cn/about/sponsor/)


**特别赞助**

Coost 由以下企业特别赞助，在此深表感谢🙏

<a href="https://www.oneflow.org/index.html">
<img src="https://coostdocs.github.io/images/sponsor/oneflow.png" width="175" height="125">
</a>




## 2. 参考文档

- 简体中文: [github](https://coostdocs.github.io/cn/about/co/) | [gitee](https://coostdocs.gitee.io/cn/about/co/)
- English: [github](https://coostdocs.github.io/en/about/co/) | [gitee](https://coostdocs.gitee.io/en/about/co/)




## 3. 核心组件


### 3.0 面向玄学编程

```cpp
#include "co/god.h"

void f() {
    god::bless_no_bugs();
}
```



### 3.1 co/flag

[co/flag](https://coostdocs.github.io/cn/co/flag/) 是一个命令行参数与配置文件解析库，用法与 [gflags](https://github.com/gflags/gflags) 类似，但功能更加强大：
- 支持从命令行、配置文件传入参数。
- 支持自动生成配置文件。
- 支持 flag 别名。
- 整数类型的 flag，值可以带单位 `k,m,g,t,p`，不分大小写。

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

上述代码中 `DEF_` 开头的宏，定义了 3 个 flag，每个 flag 对应一个全局变量，变量名是 `FLG_` 加 flag 名。上面的代码编译后，可以按下面的方式运行：

```sh
./xx                    # 按默认参数运行
./xx -x -s good         # x = true, s = "good"
./xx -i 4k -s "I'm ok"  # i = 4096, s = "I'm ok"

./xx -mkconf            # 自动生成配置文件 xx.conf
./xx xx.conf            # 从配置文件传入参数
./xx -conf xx.conf      # 与上同
```



### 3.2 co/log

[co/log](https://coostdocs.github.io/cn/co/log/) 是一个内存友好的高性能日志系统，程序运行稳定后，打印日志不需要分配内存。

co/log 支持两种类型的日志：一种是级别日志，将日志分为 debug, info, warning, error, fatal 5 个级别，**打印 fatal 级别的日志会终止程序的运行**；另一种是 TLOG，日志按 topic 分类，不同 topic 的日志写入不同的文件。

```cpp
DLOG << "hello " << 23;  // debug
LOG << "hello " << 23;   // info
WLOG << "hello " << 23;  // warning
ELOG << "hello " << 23;  // error
FLOG << "hello " << 23;  // fatal
TLOG("xx") << "s" << 23; // topic log
```

co/log 还提供了一系列 `CHECK` 宏，可以视为加强版的 `assert`，它们在 debug 模式下也不会被清除。

```cpp
void* p = malloc(32);
CHECK(p != NULL) << "malloc failed..";
CHECK_NE(p, NULL) << "malloc failed..";
```

CHECK 断言失败时，co/log 会打印函数调用栈信息，然后终止程序的运行。在 linux 与 macosx 上，需要安装 [libbacktrace](https://github.com/ianlancetaylor/libbacktrace)。

![stack](https://coostdocs.github.io/images/stack.png)

co/log 速度非常快，下面是一些测试结果，仅供参考：

- co/log vs glog (single thread)

  | platform | google glog | co/log |
  | ------ | ------ | ------ |
  | win2012 HHD | 1.6MB/s | 180MB/s |
  | win10 SSD | 3.7MB/s | 560MB/s |
  | mac SSD | 17MB/s | 450MB/s |
  | linux SSD | 54MB/s | 1023MB/s |



- [co/log vs spdlog](https://github.com/idealvin/coost/tree/benchmark/benchmark) (Linux)

  | threads | total logs | co/log time(seconds) | spdlog time(seconds)|
  | ------ | ------ | ------ | ------ |
  | 1 | 1000000 | 0.087235 | 2.076172 |
  | 2 | 1000000 | 0.183160 | 3.729386 |
  | 4 | 1000000 | 0.206712 | 4.764238 |
  | 8 | 1000000 | 0.302088 | 3.963644 |



### 3.3 co/unitest

[co/unitest](https://coostdocs.github.io/cn/co/unitest/) 是一个简单易用的单元测试框架，co 中的很多组件会用它写单元测试代码，为 co 的稳定性提供了保障。

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

上面是一个简单的例子，`DEF_test` 宏定义了一个测试单元，实际上就是一个函数(类中的方法)。`DEF_case` 宏定义了测试用例，每个测试用例实际上就是一个代码块。main 函数一般只需要下面几行：

```cpp
#include "co/unitest.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    unitest::run_all_tests();
    return 0;
}
```

[unitest](https://github.com/idealvin/coost/tree/master/unitest) 目录下面是 co 中的单元测试代码，编译后可执行下述命令运行：

```sh
xmake r unitest      # 运行所有单元测试用例
xmake r unitest -os  # 仅运行 os 单元中的测试用例
```



### 3.4 JSON

[co/json](https://coostdocs.github.io/cn/co/json/) 是一个兼具性能与易用性的 JSON 库。

```cpp
// {"a":23,"b":false,"s":"123","v":[1,2,3],"o":{"xx":0}}
Json x = {
    { "a", 23 },
    { "b", false },
    { "s", "123" },
    { "v", {1,2,3} },
    { "o", {
        {"xx", 0}
    }},
};

// equal to x
Json y = Json()
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

- [co/json vs rapidjson](https://github.com/idealvin/coost/tree/benchmark/benchmark) (Linux)

  |  | parse | stringify | parse(minimal) | stringify(minimal) |
  | ------ | ------ | ------ | ------ | ------ |
  | rapidjson | 1270 us | 2106 us | 1127 us | 1358 us |
  | co/json | 1005 us | 920 us | 788 us | 470 us |



### 3.5 协程

co 实现了类似 [golang goroutine](https://github.com/golang/go) 的协程，它有如下特性：

- 支持多线程调度，默认线程数为系统 CPU 核数。
- 共享栈，同一线程中的协程共用若干个栈(大小默认为 1MB)，内存占用低，Linux 上的测试显示 1000 万协程只用了 2.8G 内存(仅供参考)。
- 各协程之间为平级关系，可以在任何地方(包括在协程中)创建新的协程。
- 支持系统 API hook (Windows/Linux/Mac)，可以直接在协程中使用三方网络库。
- 支持协程锁 [co::Mutex](https://coostdocs.github.io/cn/co/coroutine/#%E5%8D%8F%E7%A8%8B%E9%94%81comutex)、协程同步事件 [co::Event](https://coostdocs.github.io/cn/co/coroutine/#%E5%8D%8F%E7%A8%8B%E5%90%8C%E6%AD%A5%E4%BA%8B%E4%BB%B6coevent)。
- 支持 golang 中的 channel、waitgroup 等特性：[co::Chan](https://coostdocs.github.io/cn/co/coroutine/#channelcochan)、[co::WaitGroup](https://coostdocs.github.io/cn/co/coroutine/#waitgroupcowaitgroup)。
- 支持协程池 [co::Pool](https://coostdocs.github.io/cn/co/coroutine/#%E5%8D%8F%E7%A8%8B%E6%B1%A0copool)（无锁、无原子操作）。


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

上面的代码中，`go()` 创建的协程会均匀的分配到不同的调度线程中。用户也可以自行控制协程的调度：

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



### 3.6 网络编程

co 提供了一套协程化的 [socket API](https://coostdocs.github.io/cn/co/coroutine/#%e5%8d%8f%e7%a8%8b%e5%8c%96%e7%9a%84-socket-api)，它们大部分形式上与原生的 socket API 基本一致，熟悉 socket 编程的用户，可以轻松的用同步的方式写出高性能的网络程序。

co 也实现了更高层的网络编程组件，包括 [TCP](https://coostdocs.github.io/cn/co/net/tcp/)、[HTTP](https://coostdocs.github.io/cn/co/net/http/) 以及基于 [JSON](https://coostdocs.github.io/cn/co/json/) 的 [RPC](https://coostdocs.github.io/cn/co/net/rpc/) 框架，它们兼容 IPv6，同时支持 SSL，用起来比 socket API 更方便。


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

**co/rpc 同时支持 HTTP 协议**，可以用 POST 方法调用 RPC 服务：

```sh
curl http://127.0.0.1:7788/xx --request POST --data '{"api":"ping"}'
```


- **静态 web server**

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




## 4. 代码构成

- [include](https://github.com/idealvin/coost/tree/master/include)  

  co 的头文件。

- [src](https://github.com/idealvin/coost/tree/master/src)  

  co 的源代码，编译生成 libco。

- [test](https://github.com/idealvin/coost/tree/master/test)  

  一些测试代码，每个 `.cc` 文件都会编译成一个单独的测试程序。

- [unitest](https://github.com/idealvin/coost/tree/master/unitest)  

  一些单元测试代码，每个 `.cc` 文件对应不同的测试单元，所有代码都会编译到单个测试程序中。

- [gen](https://github.com/idealvin/coost/tree/master/gen)  

  代码生成工具，根据 proto 文件，自动生成 RPC 框架代码。


## 5. 构建

### 5.1 编译器要求

编译 co 需要编译器支持 C++11：

- Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
- Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
- Windows: [vs2015+](https://visualstudio.microsoft.com/)


### 5.2 用 xmake 构建

co 推荐使用 [xmake](https://github.com/xmake-io/xmake) 作为构建工具。


#### 5.2.1 快速上手

```sh
# 所有命令都在 co 根目录执行，后面不再说明
xmake       # 默认构建 libco
xmake -a    # 构建所有项目 (libco, gen, test, unitest)
```


#### 5.2.2 基于 mingw 构建

```sh
xmake f -p mingw
xmake -v
```


#### 5.2.3 启用 HTTP/SSL 特性

```sh
xmake f --with_libcurl=true --with_openssl=true
xmake -v
```


#### 5.2.4 安装 libco

```sh
xmake install -o pkg          # 打包安装到 pkg 目录
xmake i -o pkg                # 同上
xmake install -o /usr/local   # 安装到 /usr/local 目录
```


#### 5.2.5 从 xrepo 安装 libco

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


#### 5.3.5 从 vcpkg 安装 libco

```sh
vcpkg install coost:x64-windows

# 启用 HTTP & SSL
vcpkg install coost[libcurl,openssl]:x64-windows
```


#### 5.3.6 从 conan 安装 libco

```sh
conan install coost
```


#### 5.3.7 Cmake 中查找 coost 包

```cmake
find_package(coost REQUIRED CONFIG)
target_link_libraries(userTarget coost::co)
```




## 6. License

The MIT license. Coost 包含了一些其他项目的代码，可能使用了不同的 License，详情见 [LICENSE.md](https://github.com/idealvin/coost/blob/master/LICENSE.md)。




## 7. 特别致谢

- [co/context](https://github.com/idealvin/coost/tree/master/src/co/context) 的相关代码取自 [ruki](https://github.com/waruqi) 的 [tbox](https://github.com/tboox/tbox)，特别表示感谢！
- [Leedehai](https://github.com/Leedehai) 与 [daidai21](https://github.com/daidai21) 早期帮忙将 co 的中文参考文档翻译成英文，特别表示感谢！
- [ruki](https://github.com/waruqi) 帮忙改进了 xmake 构建脚本，特别表示感谢！
- [izhengfan](https://github.com/izhengfan) 提供了 cmake 构建脚本，特别表示感谢！
- [SpaceIm](https://github.com/SpaceIm) 完善了 cmake 构建脚本，提供了 `find_package` 的支持，特别表示感谢！

