# Basic [(中文)](readme_cn.md)

`CO` is an elegant and efficient C++ base library that supports Linux, Windows and Mac platforms.

`CO` pursues minimalism and efficiency. It does not rely on third-party libraries such as [boost](https://www.boost.org/), and uses only a few C++11 features.

- CO contains the following functional components:
    - Basic definitions (def)
    - Atomic operations (atomic)
    - Fast random number generator (random)
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

- The code of [co/context](https://github.com/idealvin/co/tree/master/src/co/context) is from [tbox](https://github.com/tboox/tbox) by [ruki](https://github.com/waruqi), special thanks!
- The English reference documents of CO are translated by [Leedehai](https://github.com/Leedehai) (1-10), [daidai21](https://github.com/daidai21) (11-15) and [google](https://translate.google.cn/), special thanks!
- [ruki](https://github.com/waruqi) has helped to improve the xmake compilation scripts, thanks in particular!
- [izhengfan](https://github.com/izhengfan) provided cmake compilation scripts, thank you very much!

## Reference documents

- [English md](https://github.com/idealvin/co/tree/master/docs/en)
- [中文 md](https://github.com/idealvin/co/tree/master/docs/cn)
- [中文 pdf](https://code.aliyun.com/idealvin/docs/blob/9cb3feb78a35dc6cc1f13d6d93003f6150d047f9/pdf/co.pdf)

## Highlight function

- **[flag](https://github.com/idealvin/co/blob/master/src/flag.cc)**

  This is a command line arguments and configuration file parsing library that is really considered for programmers. It supports automatic generation of configuration files. It supports integer types with units `k, m, g, t, p`.

- **[log](https://github.com/idealvin/co/blob/master/src/log.cc)**

  This is a super fast local logging system, see how fast it is below:

  | log vs glog | google glog | co/log |
  | ------ | ------ | ------ |
  | win2012 HHD | 1.6MB/s | 180MB/s |
  | win10 SSD | 3.7MB/s | 560MB/s |
  | mac SSD | 17MB/s | 450MB/s |
  | linux SSD | 54MB/s | 1023MB/s |

  The above table is the test result of one million info log (about 50 bytes) continuously printed by a single thread. The [co/log](https://github.com/idealvin/co/blob/master/include/log.h) is almost two orders of magnitude faster than [glog](https://github.com/google/glog).

  Why is it so fast? The first is that it is based on [fastream](https://github.com/idealvin/co/blob/master/include/fastream.h) that is 8-25 times faster than `sprintf`. The second is that it has almost no memory allocation operations.

- **[json](https://github.com/idealvin/co/blob/master/src/json.cc)**

  This is a json library that is comparable to [rapidjson](https://github.com/Tencent/rapidjson) in performance. If you use [jemalloc](https://github.com/jemalloc/jemalloc), the performance of `parse` and `stringify` will be further improved.

  It does not support the json standard as comprehensively as rapidjson, but meets the basic needs of programmers and is easier to use.

- **[co](https://github.com/idealvin/co/tree/master/src/co)**

  This is a [golang-style](https://github.com/golang/go) coroutine library with built-in multi-threaded scheduling, which is a great tool for network programming.

- **[json rpc](https://github.com/idealvin/co/blob/master/src/rpc.cc)**

  This is a high-performance rpc framework based on coroutines and json. It supports automatic code generation, easy to use, and single-threaded qps can reach 120k+.

- **[Kakalot(卡卡洛特)](https://github.com/idealvin/co/blob/master/include/co/co.h)**

  This is a template class for use with `co::Pool`. `co::Pool` is a coroutine-safe pool that stores `void*` pointers internally.

  Kakalot pulls a pointer from the pool during construction and puts it back during destruction. At the same time, Kakalot also comes with smart pointer properties:

  ```cpp
  co::Pool p;
  
  void f () {
      co::Kakalot<Redis> rds(p);         // Kakalot is now a Redis* pointer
      if (rds == NULL) rds = new Redis;  // reassign Kakalot 
      rds->get("xx");                    // Kakalot calls redis's get operation
  }
  
  go(f);
  ```

## Components

- [co/include](https://github.com/idealvin/co/tree/master/include)  
  Header files of `libco`.

- [co/src](https://github.com/idealvin/co/tree/master/src)  
  Source files of `libco`.

- [co/test](https://github.com/idealvin/co/tree/master/test)  
  Some test code, each `.cc` file will be compiled into a separate test program.

- [co/unitest](https://github.com/idealvin/co/tree/master/unitest)  
  Some unit test code, each `.cc` file corresponds to a different test unit, and all code is compiled into a single test program.

- [co/gen](https://github.com/idealvin/co/tree/master/gen)  
  A code generation tool automatically generates rpc framework code according to the `proto` file.

## Compiling

### xmake

[Xmake](https://github.com/xmake-io/xmake) is recommended for compiling the `CO` project.

- Compiler
    - Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
    - Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
    - Windows: [vs2015+](https://visualstudio.microsoft.com/)

- Install xmake

  For windows, mac and debian/ubuntu, you can directly go to the [xmake release page](https://github.com/xmake-io/xmake/releases) to download the installation package. For other systems, please refer to xmake's [installation instructions](https://xmake.io/#/guide/installation).

  Xmake disables compilation as root by default on linux. [ruki](https://github.com/waruqi) says it is not safe. You can add the following line to `~/.bashrc` to enable root compilation:

  ```sh
  export XMAKE_ROOT=y
  ```

- Quick start

  ```sh
  # All commands are executed in the root directory of co (the same below)
  xmake       # build libco and gen by default
  xmake -a    # build all projects (libco, gen, co/test, co/unitest)
  ```

- Build libco

  ```sh
  xmake build libco       # build libco only
  xmake -b libco          # the same as above
  xmake b libco           # the same as above, required newer version of xmake
  ```

- Build and run unitest code

  [co/unitest](https://github.com/idealvin/co/tree/master/unitest) is unit test code that verifies the correctness of the functionality of the base library.

  ```sh
  xmake build unitest    # build can be abbreviated as -b
  xmake run unitest -a   # run all unit tests
  xmake r unitest -a     # the same as above
  xmake r unitest -os    # run unit test os
  xmake r unitest -json  # run unit test json
  ```

- Build and run test code

  [co/test](https://github.com/idealvin/co/tree/master/test) contains some test code. You can easily add a `xxx_test.cc` source file in the `co/test` directory, and then execute `xmake build xxx` in the co root directory to build it.

  ```sh
  xmake build flag       # compile flag_test.cc
  xmake build log        # compile log_test.cc
  xmake build json       # compile json_test.cc
  xmake build rapidjson  # compile rapidjson_test.cc
  xmake build rpc        # compile rpc_test.cc
  
  xmake r flag -xz       # test flag
  xmake r log            # test log
  xmake r log -cout      # also log to terminal
  xmake r log -perf      # performance test
  xmake r json           # test json
  xmake r rapidjson      # test rapidjson
  xmake r rpc            # start rpc server
  xmake r rpc -c         # start rpc client
  ```

- Build gen

  ```sh
  xmake build gen
  
  # It is recommended to put gen in the system directory (e.g. /usr/local/bin/).
  gen hello_world.proto
  ```

  Proto file format can refer to [hello_world.proto](https://github.com/idealvin/co/blob/master/test/__/rpc/hello_world.proto).

- Installation

  ```sh
  # Install header files, libco, gen by default.
  xmake install -o pkg         # package related files to the pkg directory
  xmake i -o pkg               # the same as above
  xmake install -o /usr/local  # install to the /usr/local directory
  ```

### cmake

[izhengfan](https://github.com/izhengfan) has helped to provide cmake support:  
- Build `libco` and `gen` by default.
- The library files are in the `build/lib` directory, and the executable files are in the `build/bin` directory.
- You can use `BUILD_ALL` to compile all projects.
- You can use `CMAKE_INSTALL_PREFIX` to specify the installation directory.

```sh
mkdir build && cd build
cmake ..
cmake .. -DBUILD_ALL=ON -DCMAKE_INSTALL_PREFIX=pkg
make -j8
make install
```

For more details, please refer to [here](./docs/en/compiling.md/#compile-with-cmake).

## License

`CO` is licensed under the `MIT` License. It includes code from some other projects, which have their own licenses, see details in [LICENSE.md](https://github.com/idealvin/co/blob/master/LICENSE.md).

## Contributing

1. Modify or add code in the `co/include` or `co/src` directory and ensure that the libco library compiles.
2. If necessary, modify or add unit test cases in the `co/unitest` directory and ensure that all unit tests pass.
3. If necessary, modify or add test code in the `co/test` directory.
4. All other kinds of contributions are also welcome.
