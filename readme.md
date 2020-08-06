## Basic [(中文)](readme_cn.md)

`CO` is an elegant and efficient C++ base library that supports Linux, Windows and Mac platforms. It pursues minimalism and efficiency, and does not rely on third-party library such as [boost](https://www.boost.org/).

`CO` includes coroutine library (golang-style), network library (tcp/http/rpc), log library, command line and configuration file parsing library, unit testing framework, json library and other basic components.


## Documents

- [English](https://idealvin.github.io/coding/2020/07/co_en/)
- [中文](https://idealvin.github.io/coding/2020/07/co/)


## Highlights

- **[co](https://github.com/idealvin/co/tree/master/src/co)**

  `co` is a [golang-style](https://github.com/golang/go) C++ coroutine library with the following features:

  - Support multi-thread scheduling, the default number of threads is the number of system CPU cores.
  - Coroutines share the thread stack (the default size is 1MB), and the memory footprint is extremely low, a single machine can easily create millions of coroutines.
  - Support system api hook (Linux & Mac).
  - Support coroutine lock [co::Mutex](https://github.com/idealvin/co/blob/master/src/co/impl/co.cc).
  - Support coroutine synchronization event [co::Event](https://github.com/idealvin/co/blob/master/src/co/impl/co.cc).
  - Support coroutine pool [co::Pool](https://github.com/idealvin/co/blob/master/src/co/impl/co.cc).

  -  create coroutine with `go()`:
  ```cpp
  void fun() {
      std::cout << "hello world" << std::endl;
  }
  
  go(fun);
  ```

- **[so](https://github.com/idealvin/co/tree/master/src/so)**

  `so` is a C++ network library based on coroutines. You can easily write network programs that support both `ipv4` and `ipv6` with this library. It includes the following modules:

  - tcp module, supports general tcp programming.
  - http module, supports basic http programming.
  - rpc module, implements a rpc framework based on json, single-threaded qps can reach 120k+.

  - Write a **static web server**:
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

  - Write a general http server
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

  `log` is a super fast local logging system, printing logs is safer than `printf`:
  ```cpp
  LOG << "hello "<< 23;   // info
  ELOG << "hello again";  // error
  ```

  Let's see how fast it is below:

  | log vs glog | google glog | co/log |
  | ------ | ------ | ------ |
  | win2012 HHD | 1.6MB/s | 180MB/s |
  | win10 SSD | 3.7MB/s | 560MB/s |
  | mac SSD | 17MB/s | 450MB/s |
  | linux SSD | 54MB/s | 1023MB/s |
  
  The above table is the test result of one million info logs (about 50 bytes each) continuously printed by a single thread. The [co/log](https://github.com/idealvin/co/blob/master/include/log.h) is almost two orders of magnitude faster than [glog](https://github.com/google/glog).

  Why is it so fast? The first is that it is based on [fastream](https://github.com/idealvin/co/blob/master/include/fastream.h) that is 8-25 times faster than `sprintf`. The second is that it has almost no memory allocation operations.

- **[flag](https://github.com/idealvin/co/blob/master/src/flag.cc)**

  `flag` is a command line and configuration file parsing library that supports automatic generation of configuration files.

  ```cpp
  #include "co/flag.h"

  DEF_int32(i, 32, "comments");
  DEF_string(s, "xxx", "string type");

  int main(int argc, char** argv) {
      flag::init(argc, argv);
      std::cout << "i: "<< FLG_i << std::endl;
      std::cout << "s: "<< FLG_s << std::endl;
      return 0;
  }
  ```

  Build and run:
  ```sh
  ./xx                         # start with default parameters
  ./xx -i=4k -s="hello world"  # integers can take unit k,m,g,t,p (case insensitive)
  ./xx -i 4k -s "hello world"  # equivalent to above
  ./xx --mkconf                # automatically generate configuration file xx.conf
  ./xx -config=xx.conf         # start from configuration file
  ```

- **[json](https://github.com/idealvin/co/blob/master/src/json.cc)**

  `json` is a json library comparable to [rapidjson](https://github.com/Tencent/rapidjson), if you use [jemalloc](https://github.com/jemalloc/jemalloc), the performance of `parse` and `stringify` will be further improved. This library's support for the json standard is not as comprehensive as rapidjson, but it can meet the basic needs of programmers and is easier to use.


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
  xmake build libco      # build libco only
  xmake -b libco         # the same as above
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

  [co/test](https://github.com/idealvin/co/tree/master/test) contains some test code. You can easily add a `xxx.cc` source file in the `co/test` directory, and then execute `xmake build xxx` to build it.

  ```sh
  xmake build flag             # flag.cc
  xmake build log              # log.cc
  xmake build json             # json.cc
  xmake build rapidjson        # rapidjson.cc
  xmake build rpc              # rpc.cc
  xmake build easy             # so/easy.cc
  xmake build pingpong         # so/pingpong.cc
  
  xmake r flag -xz             # test flag
  xmake r log                  # test log
  xmake r log -cout            # also log to terminal
  xmake r log -perf            # performance test
  xmake r json                 # test json
  xmake r rapidjson            # test rapidjson
  xmake r rpc                  # start rpc server
  xmake r rpc -c               # start rpc client
  xmake r easy -d xxx          # start web server
  xmake r pingpong             # pingpong server:   127.0.0.1:9988
  xmake r pingpong ip=::       # pingpong server:   :::9988  (ipv6)
  xmake r pingpong -c ip=::1   # pingpong client -> ::1:9988
  ```

- Build gen

  ```sh
  # It is recommended to put gen in the system directory (e.g. /usr/local/bin/).
  xmake build gen
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


## License

`CO` is licensed under the `MIT` License. It includes code from some other projects, which have their own licenses, see details in [LICENSE.md](https://github.com/idealvin/co/blob/master/LICENSE.md).


## Special thanks

- The code of [co/context](https://github.com/idealvin/co/tree/master/src/co/context) is from [tbox](https://github.com/tboox/tbox) by [ruki](https://github.com/waruqi), special thanks!
- The English reference documents of CO are translated by [Leedehai](https://github.com/Leedehai) (1-10), [daidai21](https://github.com/daidai21) (11-15) and [google](https://translate.google.cn/), special thanks!
- [ruki](https://github.com/waruqi) has helped to improve the xmake compilation scripts, thanks in particular!
- [izhengfan](https://github.com/izhengfan) provided cmake compilation scripts, thank you very much!


## Donate

Goto the [Donate](https://idealvin.github.io/donate/) page.
