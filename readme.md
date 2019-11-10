# Basic

`CO` 是一个优雅、高效的 C++ 基础库，支持 Linux, Windows 与 Mac 平台。  

`CO` 追求极简、高效，不依赖于 [boost](https://www.boost.org/) 等三方库，仅使用了少量 C++11 特性。

- CO 实现的功能组件：
    - 基本定义(def)
    - 原子操作(atomic)
    - 快速伪随机数生成器(ramdom)
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
    - 高效 json 库(json)
    - 高性能 json rpc 框架(rpc)
    - hash 库(hash)
    - path 库(path)
    - 文件系统操作(fs)
    - 系统操作(os)

## 编译与运行

windows 平台推荐用 `vs2015` 编译，linux、mac 平台推荐用 [scons](http://scons.org/) 编译。

- 编译器
    - Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
    - Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
    - Windows: [vs2015](https://visualstudio.microsoft.com/)

- 安装 `scons`

```sh
python setup.py install --prefix=/usr/local --no-version-script --no-install-man
```

- 编译 `libbase`

```sh
# co/lib 目录中生成 libbase.a 或 base.lib
cd co/base && scons -j4
```

- 编译 `unitest` 代码

```sh
# 生成 build/unitest
cd co/unitest/base && scons -j4

# 运行所有测试用例
./unitest -a
```

- 编译 `test` 代码

```sh
cd co/test
./_build.sh log_test.cc   # 生成 build/log.exe
./_build.sh flag_test.cc  # 生成 build/flag.exe
... ...
```
