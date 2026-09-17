# coost

[English](readme.md) | 简体中文  
[![Linux](https://img.shields.io/github/actions/workflow/status/idealvin/coost/linux.yml?branch=master&label=Linux)](https://github.com/idealvin/coost/actions?query=workflow%3ALinux)
[![Mac](https://img.shields.io/github/actions/workflow/status/idealvin/coost/macos.yml?branch=master&label=Mac)](https://github.com/idealvin/coost/actions?query=workflow%3AmacOS)
[![Windows](https://img.shields.io/github/actions/workflow/status/idealvin/coost/win.yml?branch=master&label=Windows)](https://github.com/idealvin/coost/actions?query=workflow%3AWindows)
[![Windows-arm64](https://img.shields.io/github/actions/workflow/status/idealvin/coost/win_arm64.yml?branch=master&label=Windows-arm64)](https://github.com/idealvin/coost/actions?query=workflow%3AWindows-arm64)
[![FreeBSD](https://img.shields.io/github/actions/workflow/status/idealvin/coost/freebsd.yml?branch=master&label=FreeBSD)](https://github.com/idealvin/coost/actions?query=workflow%3AFreeBSD)
[![Release](https://img.shields.io/github/release/idealvin/coost.svg)](https://github.com/idealvin/coost/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)



**[A tiny, minimalist Swiss Army knife for C++.](https://github.com/idealvin/coost)**


## 简介

**[coost](https://github.com/idealvin/coost)** 是一个轻量、高性能、易用的 C++ 基础库，支持 Linux、macOS、Windows、FreeBSD 等平台，以及 x86、x64、ARM、ARM64 等 CPU 架构。

**coost** 简称 **co**，包含 go-style 协程、网络、日志、配置解析、单元测试、基准测试、内存分配器等组件，目标是让 C++ 编程变得简单、轻松。


## 商业支持

coost 本身保持开源。如果在生产环境使用 coost，并需要定制开发、架构适配（RISC-V / MIPS）、协程 hook、性能优化、团队培训等深度支持，作者提供付费商业支持，帮助团队降低风险、节省时间。

服务套餐、报价方式、服务流程等详细信息见：

👉 [coost 商业支持](https://coostdocs.github.io/cn/about/support/)

有需求请通过 [GitHub Issues](https://github.com/idealvin/coost/issues) 或邮件 [idealvin@qq.com](mailto:idealvin@qq.com) 联系作者。首次沟通可提供一次免费诊断，用于判断问题范围与可行方案。



## 参考文档

**目前文档已落后于最新版本 coost，请以[最新代码](https://github.com/idealvin/coost)与 [include/co](https://github.com/idealvin/coost/tree/master/include/co) 头文件为准**。

- [简体中文](https://coostdocs.github.io/cn/about/co/) 
- [English](https://coostdocs.github.io/en/about/co/)



## 核心组件

### flag

**[flag](https://github.com/idealvin/coost/blob/master/include/co/flag.h)** 是一个命令行参数与配置文件解析库，支持 flag 别名、自动生成配置文件等。

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


### 协程

coost 实现了类似 golang 中 goroutine 的协程机制，它有如下特性：

- 支持多线程调度，默认调度线程数为系统 CPU 核数。
- 共享栈，同一线程中的协程共用若干个栈(大小默认为 1MB)，内存占用低。
- 支持协程同步事件、协程锁、waitgroup 等协程同步机制。

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



### 网络

coost 提供了一套基于协程的网络编程框架:

- **[Socket API](https://github.com/idealvin/coost/blob/master/include/co/sock.h)**，形式上与系统 socket API 类似，熟悉 socket 编程的用户，可以轻松用同步方式写出高性能的网络程序。
- [TCP](https://github.com/idealvin/coost/blob/master/include/co/tcp.h)，TCP服务与客户端的简单封装，兼容 IPv6。
- [RPC](https://github.com/idealvin/coost/blob/master/include/co/rpc.h)，简单的RPC框架，使用 JSON 进行序列化。



### 日志

**[log](https://github.com/idealvin/coost/blob/master/include/co/log.h)** 是一个高性能日志组件，支持在程序崩溃时打印堆栈信息。

```cpp
#include "co/log.h"

int main(int argc, char** argv) {
    flag::parse(argc, argv);
    log::info("hello ", 23);   // info
    log::warn("hello ", 23);   // warning
    log::error("hello ", 23);  // error
    log::check(1+1==2, "xx");  // 运行时断言，断言失败时，打印堆栈信息并退出程序
    return 0;
}
```

log 速度非常快，下面是一些测试结果：

| platform | glog | co/log | speedup |
| ------ | ------ | ------ | ------ |
| win2012 HDD | 1.6MB/s | 180MB/s | 112.5 |
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



### 单元测试

**[unitest](https://github.com/idealvin/coost/blob/master/include/co/unitest.h)** 是一个简单易用的单元测试框架，coost 中的很多组件用它写[单元测试代码](https://github.com/idealvin/coost/tree/master/unitest)，为 coost 的稳定性提供了重要保障。

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

上面是一个简单的例子，`DEF_test` 宏定义了一个测试单元，实际上就是一个函数。`DEF_case` 宏定义了测试用例，每个测试用例实际上就是一个代码块。

[coost/unitest](https://github.com/idealvin/coost/tree/master/unitest) 目录下面是 coost 中的单元测试代码，执行如下命令构建及运行：

```sh
xmake b unitest
xmake r unitest      # 运行所有单元测试用例
xmake r unitest -os  # 仅运行 os 单元中的测试用例, os 即单元测试名
```



### 性能基准测试

**[benchmark](https://github.com/idealvin/coost/blob/master/include/co/benchmark.h)** 是一个简单易用的性能基准测试框架。

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

[coost/benchmark](https://github.com/idealvin/coost/tree/master/benchmark) 目录下是一些性能测试代码，执行如下命令构建及运行：

```sh
xmake b benchmark
xmake r benchmark        # 默认执行所有测试代码
xmake r benchmark -mem   # 仅运行 BM_group(mem) 中的测试代码
```



### JSON

**[JSON](https://github.com/idealvin/coost/blob/master/include/co/json.h)** 采用**流畅(fluent)接口设计**，用起来更加方便。

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

下面是 co/json 与 rapidjson 的性能对比：

| os | co/json stringify | co/json parse | rapidjson stringify | rapidjson parse | speedup |
| ------ | ------ | ------ | ------ | ------ | ------ |
| win | 569 | 924 | 2089 | 2495 | 3.6/2.7 |
| mac | 783 | 1097 | 1289 | 1658 | 1.6/1.5 |
| linux | 468 | 764 | 1359 | 1070 | 2.9/1.4 |

上表是将 [twitter.json](https://raw.githubusercontent.com/simdjson/simdjson/master/jsonexamples/twitter.json) 最小化后测得的 stringify 及 parse 的平均耗时，单位为微秒(us)，speedup 是 co/json 在 stringify、parse 方面相对于 rapidjson 的性能提升倍数。


## 代码构成

- [include](https://github.com/idealvin/coost/tree/master/include)  

  coost 的头文件。

- [src](https://github.com/idealvin/coost/tree/master/src)  

  coost 的源代码，编译生成 libco。

- [benchmark](https://github.com/idealvin/coost/tree/master/benchmark)  

  性能基准测试代码，每个 `.cc` 文件对应不同的测试单元，所有代码都会编译到单个测试程序中。

- [test](https://github.com/idealvin/coost/tree/master/test)  

  测试代码，每个 `.cc` 文件都会编译成一个单独的测试程序。

- [unitest](https://github.com/idealvin/coost/tree/master/unitest)  

  单元测试代码，每个 `.cc` 文件对应不同的测试单元，所有代码都会编译到单个测试程序中。

- [gen](https://github.com/idealvin/coost/tree/master/gen)  

  代码生成工具。




## 构建

### 编译器要求

**最新版本 coost 需要编译器支持 C++17**：

- Linux: [gcc](https://gcc.gnu.org/projects/cxx-status.html#cxx17)
- Mac: [clang](https://clang.llvm.org/cxx_status.html)
- Windows: [MSVC](https://visualstudio.microsoft.com/)


### 用 xmake 构建

coost 推荐使用 [xmake](https://github.com/xmake-io/xmake) 作为构建工具。


#### 快速上手

```sh
# 所有命令都在 coost 根目录执行，后面不再说明
xmake       # 默认构建 libco
xmake -a    # 构建所有项目 (libco, benchmark, gen, test, unitest)
```

#### 启用 backtrace 特性

在 Linux、macOS 上打印程序崩溃的堆栈信息，需要 [libbacktrace](https://github.com/ianlancetaylor/libbacktrace)，Linux 上较新版本的 gcc 已经内置了 backtrace 库，macOS 上一般需要手动安装。

```sh
xmake f --with_backtrace=true
xmake b stack   # test/stack.cc
xmake r stack   # 运行 stack 测试程序
```



#### 安装 libco

```sh
xmake install -o pkg          # 打包安装到 pkg 目录
xmake i -o pkg                # 同上
xmake install -o /usr/local   # 安装到 /usr/local 目录
```


### 用 cmake 构建

#### 构建 libco

```sh
mkdir cmakebuild && cd cmakebuild
cmake ..
make -j8
```


#### 构建所有项目

```sh
mkdir cmakebuild && cd cmakebuild
cmake .. -DBUILD_ALL=ON -DCMAKE_INSTALL_PREFIX=/usr/local
make -j8
cd bin
./unitest  # 运行单元测试程序
```

#### 启用 backtrace 特性

```sh
mkdir cmakebuild && cd cmakebuild
cmake .. -DWITH_BACKTRACE=ON
make -j8
```



## License

The MIT license. coost 包含了一些其他项目的代码，可能使用了不同的 License，详情见 [LICENSE.md](https://github.com/idealvin/coost/blob/master/LICENSE.md)。



## 特别致谢

- [context](https://github.com/idealvin/coost/tree/master/src/co/context) 的相关代码取自 [ruki](https://github.com/waruqi) 的 [tbox](https://github.com/tboox/tbox)，另外ruki也帮忙改进了 xmake 构建脚本，特别表示感谢！
- [izhengfan](https://github.com/izhengfan) 提供了 cmake 构建脚本，特别表示感谢！
- [SpaceIm](https://github.com/SpaceIm) 完善了 cmake 构建脚本，提供了 `find_package` 的支持，特别表示感谢！
- [Leedehai](https://github.com/Leedehai) 与 [daidai21](https://github.com/daidai21) 早期帮忙将 coost 的中文参考文档翻译成英文，特别表示感谢！
