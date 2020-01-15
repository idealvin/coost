# Basic

- [English](readme.md)
- [中文](readme_cn.md)

`CO` 是一个优雅、高效的 C++ 基础库，支持 Linux, Windows 与 Mac 平台。  

`CO` 追求极简、高效，不依赖于 [boost](https://www.boost.org/) 等三方库，仅使用了少量 C++11 特性。

- CO 包含如下的功能组件：
    - 基本定义(def)
    - 原子操作(atomic)
    - 快速随机数生成器(ramdom)
    - LruMap
    - 基本类型快速转字符串(fast)
    - 高效字符流(fastream)
    - 高效字符串(fastring)
    - 字符串操作(str)
    - 命令行参数与配置文件解析库(flag)
    - 高效流式日志库(log)
    - 单元测试框架(unitest)
    - 时间库(time)
    - 线程库(thread)
    - 协程库(co)
    - 高效 json 库
    - 高性能 json rpc 框架
    - hash 库
    - path 库
    - 文件系统操作(fs)
    - 系统操作(os)

## 特别致谢

- co 协程库切换 context 的相关代码取自 [ruki](https://github.com/waruqi) 的 [tbox](https://github.com/tboox/tbox)，特别表示感谢！
- co 英文参考文档，由 [Leedehai](https://github.com/Leedehai)(1-10) 与 [daidai21](https://github.com/daidai21)(11-15) 翻译，特别表示感谢！
- [ruki](https://github.com/waruqi) 帮忙改进了 xmake 编译脚本，特别表示感谢！
- [izhengfan](https://github.com/izhengfan) 提供了 cmake 编译脚本，特别表示感谢！

## 参考文档

- [English md](https://github.com/idealvin/co/tree/master/docs/en)
- [中文 md](https://github.com/idealvin/co/tree/master/docs/cn)
- [中文 pdf](https://code.aliyun.com/idealvin/docs/blob/3ca20c3ea964924aef83a68d12941cbff9378588/pdf/co.pdf)

## 亮点功能

- **[flag](https://github.com/idealvin/co/blob/master/base/flag.h)**

  这是一个真正为程序员考虑的命令行参数及配置文件解析库，支持自动生成配置文件，支持整数类型带单位 `k, m, g, t, p`，不区分大小写。

- **[log](https://github.com/idealvin/co/blob/master/base/log.h)**

  这是一个超级快的本地日志系统，直观感受一下:  

  | log vs glog | google glog | co log |
  | ------ | ------ | ------ |
  | win2012 HHD | 1.6MB/s | 180MB/s |
  | win10 SSD | 3.7MB/s | 560MB/s |
  | mac SSD | 17MB/s | 450MB/s |
  | linux SSD | 54MB/s | 1023MB/s |
  
  上表是单线程连续打印 100w 条 info 日志(50 字节左右)的测试结果，[co/log](https://github.com/idealvin/co/blob/master/base/log.h) 几乎快了 [glog](https://github.com/google/glog) 两个数量级。

  为何如此快？一是 log 库内部基于比 sprintf 快 8-25 倍的 [fastream](https://github.com/idealvin/co/blob/master/base/fastream.h) 实现，二是 log 库几乎没有什么内存分配操作。

- **[json](https://github.com/idealvin/co/blob/master/base/json.h)**

  这是一个速度堪比 [rapidjson](https://github.com/Tencent/rapidjson) 的 json 库，如果使用 [jemalloc](https://github.com/jemalloc/jemalloc)，`parse` 与 `stringify` 的性能会进一步提升。

  co/json 对 json 标准的支持不如 rapidjson 全面，但能满足程序员的基本需求，且更容易使用。

- **[co](https://github.com/idealvin/co/tree/master/base/co)**

  这是一个 [golang](https://github.com/golang/go) 风格的协程库，内置多线程调度，是网络编程之利器。

- **[json rpc](https://github.com/idealvin/co/blob/master/base/rpc.h)**

  这是一个基于协程与 json 的高性能 rpc 框架，支持代码自动生成，简单易用，单线程 qps 能达到 12w+。

- **[Kakalot(卡卡洛特)](https://github.com/idealvin/co/blob/master/base/co/co.h)**

  这是一个与 `co::Pool` 配套使用的模板类。`co::Pool` 是一个协程安全的池子，内部存储 `void*` 指针。

  卡卡洛特构造时从 Pool 拉取一个指针，析构时将指针放回 Pool，同时，卡卡洛特还自带智能指针属性:

  ```cpp
  co::Pool p;
  
  void f() {
      co::Kakalot<Redis> rds(p);         // 卡卡洛特现在变身为 Redis* 指针
      if (rds == NULL) rds = new Redis;  // 对卡卡洛特重新赋值
      rds->get("xx");                    // 卡卡洛特调用 redis 的 get 操作
  }
  
  go(f);
  ```

## 编译执行

`CO` 使用 [xmake](https://github.com/xmake-io/xmake) 进行编译，后续可能不再支持 ~~[scons](https://scons.org/)~~, ~~[vs project](https://visualstudio.microsoft.com/)~~。

[izhengfan](https://github.com/izhengfan) 同学帮忙提供了 cmake 支持，若需要使用 cmake 编译，请参考[这里](./docs/cn/编译.md/#cmake-编译)。

- 编译器
    - Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
    - Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
    - Windows: [vs2015](https://visualstudio.microsoft.com/)

- 安装 xmake

  windows, mac 与 debian/ubuntu 可以直接去 xmake 的 [release](https://github.com/xmake-io/xmake/releases) 页面下载安装包，其他系统请参考 xmake 的 [Installation](https://xmake.io/#/guide/installation) 说明。

  xmake 在 linux 上默认禁止 root 用户编译，[ruki](https://github.com/waruqi) 说 root 不安全，可以在 `~/.bashrc` 中加上下面的一行，启用 root 编译:
  ```sh
  export XMAKE_ROOT=y
  ```

- 编译 libbase

  [co/base](https://github.com/idealvin/co/tree/master/base) 是 CO 的核心基础库，其他测试程序都依赖于 base 库。

  ```sh
  # 所有命令都在 co 根目录执行，后面不再说明
  xmake                  # 默认编译 libbase 与 rpcgen
  xmake --all            # 编译所有项目
  xmake build base       # 编译 libbase
  ```

- 编译及运行 unitest 代码

  [co/unitest](https://github.com/idealvin/co/tree/master/unitest/base) 是单元测试代码，用于检验 base 库功能的正确性。

  ```sh
  xmake build unitest       # build 可以简写为 -b
  xmake run unitest -a      # 执行所有单元测试，run 可以简写为 r
  xmake run unitest -os     # 执行单元测试 os
  xmake run unitest -json   # 执行单元测试 json
  ```

- 编译及运行 test 代码

  [co/test](https://github.com/idealvin/co/tree/master/test) 包含了一些测试代码。co/test 目录下增加 `xxx_test.cc` 源文件，然后在 co 根目录下执行 `xmake build xxx` 即可构建。

  ```sh
  xmake build log        # 编译 log_test.cc
  xmake build flag       # 编译 flag_test.cc
  xmake build rpc        # 编译 rpc_test.cc
  xmake build stack      # 编译 stack_test.cc
  xmake build json       # 编译 json_test.cc
  xmake build rapidjson  # 编译 rapidjson_test.cc
                         # 其他的都可以类似编译
  
  xmake run log -perf    # log 库性能测试
  xmake run rpc -c=0     # 启动 rpc server
  xmake run rpc -c       # 启动 rpc client
  xmake run flag -xz     # 执行 flag
  xmake run stack        # 测试程序崩溃时打印堆栈信息，默认在协程中执行
  xmake run stack -t     # 测试程序崩溃时打印堆栈信息，在线程中执行
  xmake run stack -m     # 直接在主线程中执行
  xmake run json         # 测试 json
  xmake run rapidjson    # 测试 rapidjson
  ```

- 编译 rpcgen

  [rpcgen](https://github.com/idealvin/co/tree/master/rpcgen) 是 `json rpc` 的代码生成器，根据指定的 proto 文件，自动生成相应的代码。

  ```sh
  xmake build rpcgen
  
  # 建议将 rpcgen 放到系统目录下(如 /usr/local/bin/).
  # 有些 linux 系统自带了一个 rpcgen，为避免冲突，可能需要重命名 rpcgen.
  rpcgen hello_world.proto
  ```

  `proto` 文件格式可以参考 [co/test/rpc/hello_world.proto](https://github.com/idealvin/co/blob/master/test/rpc/hello_world.proto)。

- 安装

  ```sh
  # 默认安装头文件、libbase、rpcgen
  xmake install -o pkg          # 打包安装到 pkg 目录
  xmake install -o /usr/local   # 安装到 /usr/local 目录
  ```

## 贡献代码

1. `co/base` 目录下修改或新增代码，确保 `base` 库编译通过.
2. 若有必要，在 `co/unitest/base` 目录下修改或新增单元测试用例，确保单元测试全部通过.
3. 若有必要，在 `co/test` 目录下修改或新增测试代码.
4. 其他形式的贡献也非常欢迎.


## 友情合作

- 有问题请提交到 [github](https://github.com/idealvin/co/).
- 赞助、商务合作请联系 `idealvin@qq.com`.
- 捐他一个亿！请用微信扫 [co.pdf](https://code.aliyun.com/idealvin/docs/blob/3ca20c3ea964924aef83a68d12941cbff9378588/pdf/co.pdf) 中的二维码.
