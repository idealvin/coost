# Basic

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

## 参考文档

- [md](https://github.com/idealvin/co/blob/master/docs.md)
- [pdf](https://code.aliyun.com/idealvin/docs/blob/de01e49fe5f971d9ce24d196ecd46dc93a0644d3/pdf/co.pdf)

## 亮点功能

- **[flag](https://github.com/idealvin/co/blob/master/base/flag.h)**

这是一个真正为程序员考虑的配置文件库，支持自动生成配置文件，支持整数类型带单位 `k, m, g, t, p`，不区分大小写。

- **[log](https://github.com/idealvin/co/blob/master/base/log.h)**

这是一个超级快的本地日志系统，直观感受一下:  

| log vs glog | google glog | co log |
| ------ | ------ | ------ |
| win2012 机械硬盘 | 1.6MB/s | 180MB/s |
| win10 ssd | 3.7MB/s | 560MB/s |
| mac ssd | 17MB/s | 450MB/s |
| linux ssd | 54MB/s | 1023MB/s |

上表是单线程连续打印 100w 条 info 日志(50 字节左右)的测试结果，[co/log](https://github.com/idealvin/co/blob/master/base/log.h) 几乎甩了 [glog](https://github.com/google/glog) 两条街。

为何如此快？一是 log 库内部基于比 sprintf 快 8-25 倍的 [fastream](https://github.com/idealvin/co/blob/master/base/fastream.h) 实现，二是 log 库几乎没有什么内存分配操作。

- **[json](https://github.com/idealvin/co/blob/master/base/json.h)**

这是一个速度堪比 [rapidjson](https://github.com/Tencent/rapidjson) 的 json 库，如果使用 [jemalloc](https://github.com/jemalloc/jemalloc)，`parse` 与 `stringify` 的速度能达到 rapidjson 的两倍。

co/json 虽不如 rapidjson 全面，但能满足程序员的基本需求，且更容易使用。

- **[co](https://github.com/idealvin/co/tree/master/base/co)**

这是一个 [golang](https://github.com/golang/go) 风格的协程库，内置多线程调度，是网络编程之良品，居家必备。

- **[json rpc](https://github.com/idealvin/co/blob/master/base/rpc.h)**

这是一个基于协程与 json 的高性能 rpc 框架，支持代码生成，简单易用，单线程 qps 能达到 12w+。

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


## 友情合作

- 有问题请提交到 [github](https://github.com/idealvin/co/).
- 赞助、商务合作请联系 `idealvin@qq.com`.
- 捐他一个亿！请用微信扫 [co.pdf](https://code.aliyun.com/idealvin/docs/blob/de01e49fe5f971d9ce24d196ecd46dc93a0644d3/pdf/co.pdf) 中的二维码.
