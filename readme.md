# Basic

- [中文](readme_cn.md)
- [English](readme.md)

`CO` is an elegant and efficient C++ base library that supports Linux, Windows and Mac platforms.

`CO` pursues minimalism and efficiency. It does not rely on third-party libraries such as [boost](https://www.boost.org/), and uses only a few C++11 features.

- CO contains the following functional components:
    - Basic definitions (def)
    - Atomic operations (atomic)
    - Fast random number generator (ramdom)
    - LruMap
    - Fast string casting for basic types (fast)
    - Efficient byte stream (fastream)
    - Efficient strings (fastring)
    - String operations (str)
    - Command line arguments and configuration file parsing library (flag)
    - Efficient streaming log library (log)
    - Unit testing framework (unitest)
    - Time library (time)
    - Thread library (thread)
    - Coroutine library (co)
    - Efficient json library
    - High-performance json rpc framework
    - Hash library
    - Path library
    - File system operations (fs)
    - System operations (os)

## Special thanks

- The relevant code for co coroutine library to switch context is taken from [ruki](https://github.com/waruqi)'s [tbox](https://github.com/tboox/tbox), special thanks!
- The English reference documents of CO are translated by [Leedehai](https://github.com/Leedehai) (1-10), [daidai21](https://github.com/daidai21) (11-15) and [google](https://translate.google.cn/), special thanks!
- [ruki](https://github.com/waruqi) has helped to improve the xmake compilation scripts, thanks in particular!
- [izhengfan](https://github.com/izhengfan) provided cmake compilation scripts, thank you very much!

## Reference documents

- [English md](https://github.com/idealvin/co/tree/master/docs/en)
- [中文 md](https://github.com/idealvin/co/tree/master/docs/cn)
- [中文 pdf](https://code.aliyun.com/idealvin/docs/blob/3ca20c3ea964924aef83a68d12941cbff9378588/pdf/co.pdf)

## Highlight function

- **[flag](https://github.com/idealvin/co/blob/master/base/flag.h)**

  This is a Command line arguments and configuration file parsing library that is really considered for programmers. It supports automatic generation of configuration files. It supports integer types with units `k, m, g, t, p`.

- **[log](https://github.com/idealvin/co/blob/master/base/log.h)**

  This is a super fast local logging system, see how fast it is below:

  | log vs glog | google glog | co log |
  | ------ | ------ | ------ |
  | win2012 HHD | 1.6MB/s | 180MB/s |
  | win10 SSD | 3.7MB/s | 560MB/s |
  | mac SSD | 17MB/s | 450MB/s |
  | linux SSD | 54MB/s | 1023MB/s |

  The above table is the test result of one million info log (about 50 bytes) continuously printed by a single thread. The [co/log](https://github.com/idealvin/co/blob/master/base/log.h) is almost two orders of magnitude faster than [glog](https://github.com/google/glog).

  Why is it so fast? The first is that the log library is based on [fastream](https://github.com/idealvin/co/blob/master/base/fastream.h) that is 8-25 times faster than sprintf. The second is that the log library has almost no memory allocation operations.

- **[json](https://github.com/idealvin/co/blob/master/base/json.h)**

  This is a json library that is comparable to [rapidjson](https://github.com/Tencent/rapidjson) in performance. If you use [jemalloc](https://github.com/jemalloc/jemalloc), the performance of `parse` and `stringify` will be further improved.

  co/json does not support the json standard as comprehensively as rapidjson, but it meets the basic needs of programmers and is easier to use.

- **[co](https://github.com/idealvin/co/tree/master/base/co)**

  This is a [golang-style](https://github.com/golang/go) coroutine library with built-in multi-threaded scheduling, which is a great tool for network programming.

- **[json rpc](https://github.com/idealvin/co/blob/master/base/rpc.h)**

  This is a high-performance rpc framework based on coroutines and json. It supports automatic code generation, easy to use, and single-threaded qps can reach 120k+.

- **[Kakalot(卡卡洛特)](https://github.com/idealvin/co/blob/master/base/co/co.h)**

  This is a template class for use with `co::Pool`. `co::Pool` is a coroutine-safe pool that stores `void*` pointers internally.

  Kakalot pulls a pointer from Pool during construction and puts it back during destruction. At the same time, Carcarrot also comes with smart pointer properties:

  ```cpp
  co::Pool p;
  
  void f () {
      co::Kakalot<Redis> rds(p);         // Kakalot is now a Redis* pointer
      if (rds == NULL) rds = new Redis;  // reassign Kakalot 
      rds->get("xx");                    // Kakalot calls redis's get operation
  }
  
  go(f);
  ```

## Compiling

[Xmake](https://github.com/xmake-io/xmake) is recommended for compiling the `CO` project. ~~[Scons](https://scons.org/)~~, ~~[vs project](https://visualstudio.microsoft.com/)~~ may not be supported in the future.

[izhengfan](https://github.com/izhengfan) has helped to provide cmake support. If you need to compile with cmake, please refer to [here](./docs/en/compiling.md/#compile-with-cmake).

- Compiler
    - Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
    - Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
    - Windows: [vs2015](https://visualstudio.microsoft.com/)

- Install xmake

  For windows, mac and debian/ubuntu, you can directly go to the [xmake release page](https://github.com/xmake-io/xmake/releases) to download the installation package. For other systems, please refer to xmake's [installation instructions](https://xmake.io/#/guide/installation).

  Xmake disables compilation as root by default on linux. [ruki](https://github.com/waruqi) says that root is not safe. You can add the following line to `~/.bashrc` to enable root compilation:

  ```sh
  export XMAKE_ROOT=y
  ```

- Compile libbase

  [co/base](https://github.com/idealvin/co/tree/master/base) is the core library of CO. Other test programs rely on the base library.

  ```sh
  # All commands are executed in the root directory of CO (the same below)
  xmake               # compile libbase and rpcgen by default
  xmake --all         # compile all projects
  xmake build base    # compile libbase only
  ```

- Compile and run unitest code

  [co/unitest](https://github.com/idealvin/co/tree/master/unitest/base) is unit test code that verifies the correctness of the functionality of the base library.

  ```sh
  xmake build unitest      # build can be abbreviated as -b
  xmake run unitest -a     # run all unit tests, run can be abbreviated as r
  xmake run unitest -os    # run unit test os
  xmake run unitest -json  # run unit test json
  ```

- Compile and run test code

  [co/test](https://github.com/idealvin/co/tree/master/test) contains some test code. You can easily add a `xxx_test.cc` source file in the `co/test` directory, and then execute `xmake build xxx` in the co root directory to build it.

  ```sh
  xmake build log        # compile log_test.cc
  xmake build flag       # compile flag_test.cc
  xmake build rpc        # compile rpc_test.cc
  xmake build stack      # compile stack_test.cc
  xmake build json       # compile json_test.cc
  xmake build rapidjson  # compile rapidjson_test.cc
                         # others can be compiled similarly
  
  xmake run log -perf    # log library performance test
  xmake run rpc -c = 0   # start rpc server
  xmake run rpc -c       # start rpc client
  xmake run flag -xz     # execute flag
  xmake run stack        # Print the stack information when the test program crashes, executed in coroutine by default.
  xmake run stack -t     # Print the stack information when the test program crashes, executed in a thread
  xmake run stack -m     # execute directly in the main thread
  xmake run json         # test json
  xmake run rapidjson    # test rapidjson
  ```

- Compile rpcgen

  [rpcgen](https://github.com/idealvin/co/tree/master/rpcgen) is a code generator for `json rpc`, which automatically generates the corresponding code according to the specified proto file.

  ```sh
  xmake build rpcgen
  
  # It is recommended to put rpcgen in the system directory (e.g. /usr/local/bin/).
  # Some linux systems come with an rpcgen. To avoid conflicts, you may need to rename rpcgen.
  rpcgen hello_world.proto
  ```

  Proto file format can refer to [co/test/rpc/hello_world.proto](https://github.com/idealvin/co/blob/master/test/rpc/hello_world.proto).

- Installation

  ```sh
  # Install header files, libbase, rpcgen by default.
  xmake install -o pkg         # Package related files to the pkg directory.
  xmake install -o /usr/local  # Install to the /usr/local directory.
  ```

## Contributing

1. Modify or add code in the `co/base` directory and ensure that the base library compiles.
2. If necessary, modify or add unit test cases in the `co/unitest/base` directory and ensure that all unit tests pass.
3. If necessary, modify or add test code in the `co/test` directory.
4. All other kinds of contributions are also welcome.
