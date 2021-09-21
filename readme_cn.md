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
  <span style="border-bottom: 3px double #ccc">A go-style coroutine library in C++11 and more.</span>
  </div>
</div>


## Basic [(English)](readme.md)

CO 是一个优雅、高效的 C++ 基础库，支持 Linux, Windows 与 Mac 等平台，它实现了类似 golang 的协程、基于协程的网络编程框架、命令行参数与配置文件解析库、高性能日志库、单元测试框架、JSON 库等一系列高质量的基础组件。




## 赞助

如果您喜欢 CO，可以考虑[赞助 CO](https://github.com/sponsors/idealvin)，或者[给作者来杯咖啡](https://idealvin.gitee.io/cn/about/sponsor/)，非常感谢🙏




## 参考文档

- 中文: [github](https://idealvin.github.io/cn/about/co/) [gitee](https://idealvin.gitee.io/cn/about/co/)
- English: [github](https://idealvin.github.io/en/about/co/) [gitee](https://idealvin.gitee.io/en/about/co/)




## 核心组件


### 协程

CO 实现了类似 [golang](https://github.com/golang/go) 的协程，它有如下特性：

- 多线程调度，默认线程数为系统 CPU 核数。
- 共享栈，同一线程中的协程共用若干个栈(大小默认为 1MB)，内存占用低，Linux 上的测试显示 1000 万协程只用了 2.8G 内存(仅供参考)。
- 各协程之间为平级关系，可以在任何地方(包括在协程中)创建新的协程。
- 支持系统 API hook (Windows/Linux/Mac)，可以直接在协程中使用三方网络库。
- 协程化的 [socket API](https://idealvin.github.io/cn/co/coroutine/#%E5%8D%8F%E7%A8%8B%E5%8C%96%E7%9A%84-socket-api)。
- 协程同步事件 [co::Event](https://idealvin.github.io/cn/co/coroutine/#%E5%8D%8F%E7%A8%8B%E5%90%8C%E6%AD%A5%E4%BA%8B%E4%BB%B6coevent)。
- 协程锁 [co::Mutex](https://idealvin.github.io/cn/co/coroutine/#%E5%8D%8F%E7%A8%8B%E9%94%81comutex)。
- 协程池 [co::Pool](https://idealvin.github.io/cn/co/coroutine/#%E5%8D%8F%E7%A8%8B%E6%B1%A0copool)。
- channel [co::Chan](https://idealvin.github.io/cn/co/coroutine/#channelcochan)。
- waitgroup [co::WaitGroup](https://idealvin.github.io/cn/co/coroutine/#waitgroupcowaitgroup)。


#### 创建协程

```cpp
go(ku);            // void ku();
go(f, 7);          // void f(int);
go(&T::f, &o);     // void T::f(); T o;
go(&T::f, &o, 7);  // void T::f(int); T o;
go([](){
    LOG << "hello go";
});
```

上面是用 `go()` 创建协程的例子，go() 是一个函数，它接受 1 到 3 个参数，第一个参数 `f` 是任意可调用的对象，这些参数只要满足 `f()`, `(*f)()`, `f(p)`, `(*f)(p)`, `(o->*f)()` 或者 `(o->*f)(p)` 能被调用就可以了。

`go()` 创建的协程会均匀的分配到不同的调度线程中。如果用户想让某些协程运行在同一个线程下，可以用下面的方式创建协程：

```cpp
auto s = co::next_scheduler();
s->go(f1);
s->go(f2);
```

如果用户想在所有的调度线程中创建协程，可以用下面的方式：

```cpp
auto& s = co::all_schedulers();
for (size_t i = 0; i < s.size(); ++i) {
    s[i]->go(f);
}
```


#### channel

[co::Chan](https://idealvin.github.io/cn/co/coroutine/#channelcochan)，类似于 golang 中的 channel，可用于在协程之间传递数据。

```cpp
#include "co/co.h"

DEF_main(argc, argv) {
    co::Chan<int> ch;
    go([ch]() {
        ch << 7;
    });

    int v = 0;
    ch >> v;
    LOG << "v: " << v;

    return 0;
}
```

**channel 的读写操作必须在协程中进行**，因此上述代码中用 `DEF_main` 定义 main 函数，让 main 函数中的代码也运行在协程中。

代码中的 channel 对象在栈上，而 CO 采用的是共享栈实现方式，一个协程栈上的数据可能被其他协程覆盖，**协程间一般不能直接通过栈上的数据通信**，因此代码中的 lambda 采用了**按值捕获**的方式，将 channel 拷贝了一份，传递到新建的协程中。channel 的拷贝操作只是将内部引用计数加 1，几乎不会对性能造成影响。

创建 channel 时可以像下面这样加上超时时间：

```cpp
co::Chan<int> ch(8, 1000);
```

channel 读写操作结束后，可以调用 `co::timeout()` 判断是否超时，这种方式比 golang 中基于 select 的实现方式更简单。

CO 中的 channel 基于内存拷贝实现，传递的数据类型可以是内置类型、指针类型，或者**拷贝操作具有简单的内存拷贝语义的结构体类型**。像 `std::string` 或 STL 中的容器类型，拷贝操作不是简单的内存拷贝，一般不能直接在 channel 中传递，详情见 [co::Chan 参考文档](https://idealvin.github.io/cn/co/coroutine/#channelcochan)。


#### waitgroup

[co::WaitGroup](https://idealvin.github.io/cn/co/coroutine/#waitgroupcowaitgroup)，类似于 golang 中的 `sync.WaitGroup`，可用于等待协程或线程的退出。

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



### 网络编程

CO 提供了一套协程化的 [socket API](https://idealvin.github.io/cn/co/coroutine/#%E5%8D%8F%E7%A8%8B%E5%8C%96%E7%9A%84-socket-api)，它们大部分形式上与原生的 socket API 基本一致，熟悉 socket 编程的用户，可以轻松的用同步的方式写出高性能的网络程序。另外，CO 也实现了更高层的网络编程组件，包括 [TCP](https://idealvin.github.io/cn/co/net/tcp/)、[HTTP](https://idealvin.github.io/cn/co/net/http/) 以及基于 [JSON](https://idealvin.github.io/cn/co/json/) 的 [RPC](https://idealvin.github.io/cn/co/net/rpc/) 框架，它们兼容 IPv6，同时支持 SSL，用起来比 socket API 更方便。这里只简单的展示一下 HTTP 的用法，其余的可以查看参考文档。


**静态 web server**

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

[co/flag](https://idealvin.github.io/cn/co/flag/) 是一个简单易用的命令行参数与配置文件解析库，CO 中的一些组件会用它定义配置项。

co/flag 为每个配置项提供一个默认值，在没有配置参数的情况下，程序可以按默认配置运行。用户也可以从**命令行或配置文件**传入配置参数，在需要配置文件时，可以执行 `./exe -mkconf` **自动生成配置文件**。

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

    COUT << "x: " << FLG_x;
    COUT << "y: " << FLG_y;
    COUT << "u32: " << FLG_u32;
    COUT << FLG_s << "|" << FLG_s.size();

    return 0;
}
```

上面是一个使用 co/flag 的例子，代码中 `DEF_` 开头的宏，定义了 4 个配置项，每个配置项相当于一个全局变量，变量名是 `FLG_` 加配置名。上面的代码编译完后，可以按下面的方式运行：

```sh
./xx                  # 按默认配置运行
./xx -xy -s good      # 单字母命名的 bool flag, 可以一并设置为 true
./xx -s "I'm ok"      # 含空格的字符串
./xx -u32 8k          # 整数可以带单位: k,m,g,t,p, 不区分大小写

./xx -mkconf          # 自动生成配置文件 xx.conf
./xx xx.conf          # 从配置文件传入参数
./xx -config xx.conf  # 与上同
```



### co/log

[co/log](https://idealvin.github.io/cn/co/log/) 是一个内存友好的高性能本地日志系统，程序运行稳定后，打印日志不需要分配内存。CO 中的一些组件会用它打印日志。

co/log 将日志分为 debug, info, warning, error, fatal 5 个级别，**打印 fatal 级别的日志会终止程序的运行**。用户可以像下面这样打印不同级别的日志：

```cpp
DLOG << "hello " << 23;  // debug
LOG << "hello " << 23;   // info
WLOG << "hello " << 23;  // warning
ELOG << "hello " << 23;  // error
FLOG << "hello " << 23;  // fatal
```

co/log 还提供了一系列 `CHECK` 宏，可以视为加强版的 `assert`，它们在 debug 模式下也不会被清除。

```cpp
void* p = malloc(32);
CHECK(p != NULL) << "malloc failed..";
CHECK_NE(p, NULL) << "malloc failed..";
```

CHECK 断言失败时，co/log 会打印函数调用栈信息，然后终止程序的运行。在 linux 与 macosx 上，需要安装 [libbacktrace](https://github.com/ianlancetaylor/libbacktrace)。

![stack](https://idealvin.github.io/images/stack.png)

co/log 速度非常快，在程序运行稳定后，几乎不需要内存分配操作。下面是一些测试结果，仅供参考：

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

[co/unitest](https://idealvin.github.io/cn/co/unitest/) 是一个简单易用的单元测试框架，CO 中的很多组件会用它写单元测试代码，为 CO 的稳定性提供了保障。

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

上面是一个简单的例子，`DEF_test` 宏定义了一个测试单元，实际上就是一个函数(类中的方法)。`DEF_case` 宏定义了测试用例，每个测试用例实际上就是一个代码块。多个测试单元可以放到同一个 C++ 项目中，main 函数一般只需要下面几行：

```cpp
#include "co/unitest.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    unitest::run_all_tests();
    return 0;
}
```

[co/unitest](https://github.com/idealvin/co/tree/master/unitest) 目录下面是 CO 中的单元测试代码，编译后可执行下述命令运行：

```sh
xmake r unitest -a   # 运行所有单元测试用例
xmake r unitest -os  # 仅运行 os 单元中的测试用例
```




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

  代码生成工具，根据 proto 文件，自动生成 RPC 框架代码。


## 构建

### 编译器要求

- 编译器
  - Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
  - Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
  - Windows: [vs2015+](https://visualstudio.microsoft.com/)


### xmake

co 推荐使用 [xmake](https://github.com/xmake-io/xmake) 作为构建工具。


#### 快速上手

```sh
# 所有命令都在 co 根目录执行，后面不再说明
xmake       # 默认构建 libco
xmake -a    # 构建所有项目 (libco, gen, co/test, co/unitest)
```


#### 用 mingw 构建

```sh
xmake f -p mingw
xmake -v
```


#### 启用 HTTP/SSL 特性

```sh
xmake f --with_libcurl=true --with_openssl=true
xmake
```


#### 安装 libco

```sh
xmake install -o pkg          # 打包安装到 pkg 目录
xmake i -o pkg                # 同上
xmake install -o /usr/local   # 安装到 /usr/local 目录
```


#### 从 xmake repo 安装 libco

```sh
xrepo install -f "openssl=true,libcurl=true" co
```



### cmake

[izhengfan](https://github.com/izhengfan) 帮忙提供了 cmake 支持:  


#### 默认构建 libco

```sh
mkdir build && cd build
cmake ..
make -j8
```


#### 构建所有项目

```sh
mkdir build && cd build
cmake .. -DBUILD_ALL=ON -DCMAKE_INSTALL_PREFIX=/usr/local
make -j8
make install
```


#### 启用 HTTP & SSL (需要 openssl 1.1.0 或以上版本)

```sh
mkdir build && cd build
cmake .. -DBUILD_ALL=ON -DWITH_LIBCURL=ON
make -j8
```


#### 从 vcpkg 安装 libco

```sh
vcpkg install co:x64-windows

# 启用 HTTP & SSL
vcpkg install co[libcurl,openssl]:x64-windows
```




## License

The MIT license. co 包含了一些其他项目的代码，可能使用了不同的 License，详情见 [LICENSE.md](https://github.com/idealvin/co/blob/master/LICENSE.md)。




## 特别致谢

- [co/context](https://github.com/idealvin/co/tree/master/src/co/context) 的相关代码取自 [ruki](https://github.com/waruqi) 的 [tbox](https://github.com/tboox/tbox)，特别表示感谢！
- co 英文参考文档，由 [Leedehai](https://github.com/Leedehai)(1-10)，[daidai21](https://github.com/daidai21)(11-15) 与 [google](https://translate.google.cn/) 翻译，特别表示感谢！
- [ruki](https://github.com/waruqi) 帮忙改进了 xmake 构建脚本，特别表示感谢！
- [izhengfan](https://github.com/izhengfan) 提供了 cmake 构建脚本，特别表示感谢！

