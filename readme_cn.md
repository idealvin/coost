## Ads

co is being sponsored by the following tool; please help to support us by taking a look and signing up to a free trial.

<a href="https://tracking.gitads.io/?repo=co">
 <img src="https://images.gitads.io/co" alt="GitAds"/> 
</a>


## Basic [(English)](readme.md)

`CO` 是一个优雅、高效的 C++ 基础库，支持 Linux, Windows 与 Mac 平台。`CO` 追求极简、高效，不依赖于 [boost](https://www.boost.org/) 等三方库。

`CO` 包含协程库(golang-style)、网络库(tcp/http/rpc)、日志库、命令行与配置文件解析库、单元测试框架、json 库等基本组件。


## 参考文档

- [中文](https://idealvin.github.io/coding/2020/07/co/)
- [English](https://idealvin.github.io/coding/2020/07/co_en/)


## 亮点功能

- **[co](https://github.com/idealvin/co/tree/master/src/co)**

  `co` 是一个 [golang](https://github.com/golang/go) 风格的 C++ 协程库，有如下特性:
  - 支持多线程调度，默认线程数为系统 CPU 核数.
  - 协程共享线程栈(默认大小为 1MB)，内存占用极低，单机可轻松创建数百万协程.
  - 支持系统 api hook (Linux & Mac).
  - 支持协程锁 [co::Mutex](https://github.com/idealvin/co/blob/master/src/co/impl/co.cc).
  - 支持协程同步事件 [co::Event](https://github.com/idealvin/co/blob/master/src/co/impl/co.cc).
  - 支持协程池 [co::Pool](https://github.com/idealvin/co/blob/master/src/co/impl/co.cc).

  - 用 `go()` 创建协程:
  ```cpp
  void fun() {
      std::cout << "hello world" << std::endl;
  }

  go(fun);
  ```

- **[so](https://github.com/idealvin/co/tree/master/src/so)**

  `so` 是基于协程的 C++ 网络库，可轻松实现同时支持 `ipv4` 与 `ipv6` 的网络程序，包含如下几个模块:
  - tcp 模块, 支持一般的 tcp 编程.
  - http 模块, 支持基本的 http 编程.
  - rpc 模块，基于 json 的 rpc 框架，单线程 qps 可达到 12w+.

  - 实现静态 **web server**:
  ```cpp
  #include "co/flag.h"
  #include "co/log.h"
  #include "co/so.h"

  DEF_string(d, ".", "root dir"); // 指定 web server 根目录

  int main(int argc, char** argv) {
      flag::init(argc, argv);
      log::init();

      so::easy(FLG_d.c_str()); // mum never have to worry again

      return 0;
  }
  ```

  - 实现一般的 http server:
  ```cpp
  http::Server serv("0.0.0.0", 80);

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
              res.set_status(501);
          }
      }
  );

  serv.start();
  ```

- **[log](https://github.com/idealvin/co/blob/master/src/log.cc)**

  `log` 是一个超级快的本地日志系统，打印日志比 `printf` 更安全:
  ```cpp
  LOG << "hello " << 23;  // info
  ELOG << "hello again";  // error
  ```

  下面直观感受一下 `log` 的性能:  

  | log vs glog | google glog | co/log |
  | ------ | ------ | ------ |
  | win2012 HHD | 1.6MB/s | 180MB/s |
  | win10 SSD | 3.7MB/s | 560MB/s |
  | mac SSD | 17MB/s | 450MB/s |
  | linux SSD | 54MB/s | 1023MB/s |
  
  上表是单线程连续打印 100 万条 info 日志(每条 50 字节左右)的测试结果，[co/log](https://github.com/idealvin/co/blob/master/include/log.h) 几乎快了 [glog](https://github.com/google/glog) 两个数量级。

  为何如此快？一是 log 库内部基于比 `sprintf` 快 8-25 倍的 [fastream](https://github.com/idealvin/co/blob/master/include/fastream.h) 实现，二是 log 库几乎没有什么内存分配操作。

- **[flag](https://github.com/idealvin/co/blob/master/src/flag.cc)**

  `flag` 是一个方便、易用的命令行及配置文件解析库，支持自动生成配置文件。

  ```cpp
  #include "co/flag.h"

  DEF_int32(i, 32, "comments");
  DEF_string(s, "xxx", "string type");

  int main(int argc, char** argv) {
      flag::init(argc, argv);
      std::cout << "i: " << FLG_i << std::endl;
      std::cout << "s: " << FLG_s << std::endl;
      return 0;
  }
  ```

  编译后运行:
  ```sh
  ./xx                          # 以默认参数启动
  ./xx -i=4k -s="hello world"   # 整数类型可以带单位 k,m,g,t,p, 不分大小写
  ./xx -i 4k -s "hello world"   # 与上等价
  ./xx --mkconf                 # 自动生成配置文件 xx.conf
  ./xx -config=xx.conf          # 从配置文件启动
  ```

- **[json](https://github.com/idealvin/co/blob/master/src/json.cc)**

  `json` 是一个速度堪比 [rapidjson](https://github.com/Tencent/rapidjson) 的 json 库，如果使用 [jemalloc](https://github.com/jemalloc/jemalloc)，`parse` 与 `stringify` 的性能会进一步提升。此库对 json 标准的支持不如 rapidjson 全面，但能满足程序员的基本需求，且更容易使用。


## 代码构成

- [co/include](https://github.com/idealvin/co/tree/master/include)  

  `libco` 的头文件。

- [co/src](https://github.com/idealvin/co/tree/master/src)  

  `libco` 的源代码。

- [co/test](https://github.com/idealvin/co/tree/master/test)  

  一些测试代码，每个 `.cc` 文件都会编译成一个单独的测试程序。

- [co/unitest](https://github.com/idealvin/co/tree/master/unitest)  

  一些单元测试代码，每个 `.cc` 文件对应不同的测试单元，所有代码都会编译到单个测试程序中。

- [co/gen](https://github.com/idealvin/co/tree/master/gen)  

  代码生成工具，根据 proto 文件，自动生成 rpc 框架代码。


## 编译执行

### xmake

`CO` 推荐使用 [xmake](https://github.com/xmake-io/xmake) 进行编译。

- 编译器
    - Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
    - Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
    - Windows: [vs2015+](https://visualstudio.microsoft.com/)

- 安装 xmake

  windows, mac 与 debian/ubuntu 可以直接去 xmake 的 [release](https://github.com/xmake-io/xmake/releases) 页面下载安装包，其他系统请参考 xmake 的 [Installation](https://xmake.io/#/guide/installation) 说明。

  xmake 在 linux 上默认禁止 root 用户编译，[ruki](https://github.com/waruqi) 说不安全，可以在 `~/.bashrc` 中加上下面的一行，启用 root 编译:
  ```sh
  export XMAKE_ROOT=y
  ```

- 快速上手

  ```sh
  # 所有命令都在 co 根目录执行，后面不再说明
  xmake       # 默认编译 libco 与 gen
  xmake -a    # 编译所有项目 (libco, gen, co/test, co/unitest)
  ```

- 编译 libco

  ```sh
  xmake build libco       # 编译 libco
  xmake -b libco          # 与上同
  ```

- 编译及运行 unitest 代码

  [co/unitest](https://github.com/idealvin/co/tree/master/unitest) 是单元测试代码，用于检验 libco 库功能的正确性。

  ```sh
  xmake build unitest     # build 可以简写为 -b
  xmake run unitest -a    # 执行所有单元测试
  xmake r unitest -a      # 同上
  xmake r unitest -os     # 执行单元测试 os
  xmake r unitest -json   # 执行单元测试 json
  ```

- 编译及运行 test 代码

  [co/test](https://github.com/idealvin/co/tree/master/test) 包含了一些测试代码。co/test 目录下增加 `xxx.cc` 源文件，然后在 co 根目录下执行 `xmake build xxx` 即可构建。

  ```sh
  xmake build flag             # 编译 flag.cc
  xmake build log              # 编译 log.cc
  xmake build json             # 编译 json.cc
  xmake build rapidjson        # 编译 rapidjson.cc
  xmake build rpc              # 编译 rpc.cc
  xmake build easy             # 编译 so/easy.cc
  xmake build pingpong         # 编译 so/pingpong.cc
  
  xmake r flag -xz             # 测试 flag 库
  xmake r log                  # 测试 log 库
  xmake r log -cout            # 终端也打印日志
  xmake r log -perf            # log 库性能测试
  xmake r json                 # 测试 json
  xmake r rapidjson            # 测试 rapidjson
  xmake r rpc                  # 启动 rpc server
  xmake r rpc -c               # 启动 rpc client
  xmake r easy -d xxx          # 启动 web server
  xmake r pingpong             # pingpong server:   127.0.0.1:9988
  xmake r pingpong ip=::       # pingpong server:   :::9988  (ipv6)
  xmake r pingpong -c ip=::1   # pingpong client -> ::1:9988
  ```

- 编译 gen

  ```sh
  # 建议将 gen 放到系统目录下(如 /usr/local/bin/).
  xmake build gen
  gen hello_world.proto
  ```

  `proto` 文件格式可以参考 [hello_world.proto](https://github.com/idealvin/co/blob/master/test/__/rpc/hello_world.proto)。

- 安装

  ```sh
  # 默认安装头文件、libco、gen
  xmake install -o pkg          # 打包安装到 pkg 目录
  xmake i -o pkg                # 同上
  xmake install -o /usr/local   # 安装到 /usr/local 目录
  ```

### cmake

[izhengfan](https://github.com/izhengfan) 帮忙提供了 cmake 支持:  
- 默认只编译 `libco` 与 `gen`.
- 编译生成的库文件在 build/lib 目录下，可执行文件在 build/bin 目录下.
- 可以用 `BUILD_ALL` 指定编译所有项目.
- 可以用 `CMAKE_INSTALL_PREFIX` 指定安装目录.

```sh
mkdir build && cd build
cmake ..
cmake .. -DBUILD_ALL=ON -DCMAKE_INSTALL_PREFIX=pkg
make -j8
make install
```


## License

`CO` 以 `MIT` License 发布. `CO` 包含了一些其他项目的代码，可能使用了与 `CO` 不同的 License，详情见 [LICENSE.md](https://github.com/idealvin/co/blob/master/LICENSE.md)。


## 特别致谢

- [co/context](https://github.com/idealvin/co/tree/master/src/co/context) 的相关代码取自 [ruki](https://github.com/waruqi) 的 [tbox](https://github.com/tboox/tbox)，特别表示感谢！
- co 英文参考文档，由 [Leedehai](https://github.com/Leedehai)(1-10)，[daidai21](https://github.com/daidai21)(11-15) 与 [google](https://translate.google.cn/) 翻译，特别表示感谢！
- [ruki](https://github.com/waruqi) 帮忙改进了 xmake 编译脚本，特别表示感谢！
- [izhengfan](https://github.com/izhengfan) 提供了 cmake 编译脚本，特别表示感谢！


## 友情合作

- 有问题请提交到 [github](https://github.com/idealvin/co/).
- 赞助、商务合作请联系 `idealvin at qq.com`.
- [Donate](https://idealvin.github.io/donate/)
