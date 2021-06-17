## Basic [(中文)](readme_cn.md)

co is an elegant and efficient C++ base library that supports Linux, Windows and Mac platforms. It contains a golang-style coroutine library, network library, log library, command line and configuration file parsing library, unit test framework, JSON library and other basic components.

co follows a minimalist design concept, and the interfaces provided are as simple and clear as possible, and users can easily get started. co tries to avoid excessive encapsulation which may introduce too many concepts, to reduce the learning burden of users. For example, the coroutineized socket API provided by co is nearly the same in form as the native socket API. Users who are familiar with socket programming nearly need no more cost of learning, and can easily use these APIs to write high-performance network programs.


## Documents

- [English](https://www.yuque.com/idealvin/co_en)
- [中文](https://www.yuque.com/idealvin/co)


## Highlights

### Coroutine (co)

[co](https://github.com/idealvin/co/blob/master/include/co/co.h) is a [go-style](https://github.com/golang/go) C++ coroutine library with the following features:

- Multi-thread scheduling, the default number of threads is the number of system CPU cores.
- Coroutines share the thread stack (default size is 1MB), and the memory footprint is low, a single machine can easily create millions of coroutines.
- System api hook (Linux & Mac).
- Coroutine lock [co::Mutex](https://github.com/idealvin/co/blob/master/include/co/co/mutex.h).
- Coroutine synchronization event [co::Event](https://github.com/idealvin/co/blob/master/include/co/co/event.h).
- Coroutine Pool [co::Pool](https://github.com/idealvin/co/blob/master/include/co/co/pool.h).
- Coroutineized [socket API](https://github.com/idealvin/co/blob/master/include/co/co/sock.h).

- create coroutine with `go()`:
  ```cpp
  void ku() {
      LOG << "hello world";
  }

  void gg(int v) {
      LOG << "hello "<< v;
  }

  go(ku);  // Goku
  go(gg, 777);
  ```


### Network (so)

[so](https://github.com/idealvin/co/blob/master/include/co/so) is a C++ network library based on coroutines. It provides a general ipv6-compatible TCP framework and implements a simple json-based rpc framework, and also provides an optional support for HTTP, HTTPS and SSL.

- Simple static web server
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

- http server ([openssl](https://www.openssl.org/) required for https)
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

- http client ([libcurl](https://curl.se/libcurl/) & zlib required)
  ```cpp
  http::Client c("http://127.0.0.1:7777"); // http
  http::Client c("https://github.com");    // https, openssl required
  c.add_header("hello", "world");          // add headers here

  c.get("/");
  LOG << "response code: "<< c.response_code();
  LOG << "body size: "<< c.body_size();
  LOG << "Content-Length: "<< c.header("Content-Length");
  LOG << c.header();

  c.post("/hello", "data xxx");
  LOG << "response code: "<< c.response_code();
  ```


### Log library (log)

[log](https://github.com/idealvin/co/blob/master/include/co/log.h) is a high-performance local log system.

- Print logs
  ```cpp
  LOG << "hello " << 23; // info
  DLOG << "hello" << 23; // debug
  WLOG << "hello" << 23; // warning
  ELOG << "hello again"; // error
  ```

- Performance vs glog
  | log vs glog | google glog | co/log |
  | ------ | ------ | ------ |
  | win2012 HHD | 1.6MB/s | 180MB/s |
  | win10 SSD | 3.7MB/s | 560MB/s |
  | mac SSD | 17MB/s | 450MB/s |
  | linux SSD | 54MB/s | 1023MB/s |
  
The above table is the test result of one million info logs (about 50 bytes each) continuously printed by a single thread. The [co/log](https://github.com/idealvin/co/blob/master/include/log.h) is almost two orders of magnitude faster than [glog](https://github.com/google/glog).

Why is it so fast? The first is that it is based on [fastream](https://github.com/idealvin/co/blob/master/include/fastream.h) that is 8-25 times faster than `sprintf`. The second is that it has almost no memory allocation operations.


### Command line and configuration file parsing (flag)

[flag](https://github.com/idealvin/co/blob/master/include/co/flag.h) is a convenient and easy-to-use command line and configuration file parsing library that supports automatic generation of configuration files.

- Code example
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

- Build and run
  ```sh
  ./xx                         # start with default config values
  ./xx -i=4k -s="hello world"  # integer types can take units k,m,g,t,p, non-case sensitive
  ./xx -i 4k -s "hello world"  # same as above
  ./xx --mkconf                # generate configuration file xx.conf
  ./xx xx.conf                 # start from configuration file
  ./xx -config xx.conf         # start from configuration file
  ```


### JSON

[JSON](https://github.com/idealvin/co/blob/master/include/json.h) is an easy-to-use, high-performance JSON library. The latest version stores the JSON object in a piece of contiguous memory, nearly no memory allocation is needed during pasing Json from a string, which greatly improves the parsing speed(GB per second).

- Code example
  ```cpp
  #include "co/json.h"

  // Json: {"hello":"json", "array":[123, 3.14, true, "nice"]}
  json::Root r;
  r.add_member("hello", "json");        // add key:value pair

  json::Value a = r.add_array("array"); // add key:array
  a.push_back(123, 3.14, true, "nice"); // push value to array, accepts any number of parameters

  COUT << a[0].get_int();               // 123
  COUT << r["array"][0].get_int();      // 123
  COUT << r["hello"].get_string();      // "json"

  fastring s = r.str();                 // convert Json to string
  fastring p = r.pretty();              // convert Json to human-readable string
  json::Root x = json::parse(s);        // parse Json from a string
  ```


## Code composition

- [co/include](https://github.com/idealvin/co/tree/master/include)

  Header files of `libco`.

- [co/src](https://github.com/idealvin/co/tree/master/src)

  Source files of `libco`.

- [co/test](https://github.com/idealvin/co/tree/master/test)

  Some test codes, each `.cc` file will be compiled into a separate test program.

- [co/unitest](https://github.com/idealvin/co/tree/master/unitest)

  Some unit test codes, each `.cc` file corresponds to a different test unit, and all codes will be compiled into a single test program.

- [co/gen](https://github.com/idealvin/co/tree/master/gen)

  A code generation tool automatically generates rpc framework code according to the proto file.


## Building

### xmake

co recommends using [xmake](https://github.com/xmake-io/xmake) as the build tool.

- Compiler
  - Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
  - Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
  - Windows: [vs2015+](https://visualstudio.microsoft.com/)

- Install xmake

  For windows, mac and debian/ubuntu, you can go directly to the [release page of xmake](https://github.com/xmake-io/xmake/releases) to get the installation package. For other systems, please refer to xmake's [Installation instructions](https://xmake.io/#/guide/installation).

  Xmake disables compiling as root by default on linux. [ruki](https://github.com/waruqi) says it is not safe. You can add the following line to `~/.bashrc` to enable it:
  ```sh
  export XMAKE_ROOT=y
  ```

- Quick start

  ```sh
  # All commands are executed in the root directory of co (the same below)
  xmake      # build libco and gen by default
  xmake -a   # build all projects (libco, gen, co/test, co/unitest)
  ```

- build with libcurl, openssl

  Users may build with libcurl and openssl to enable the whole HTTP & SSL features.

  ```sh
  xmake f --with_libcurl=true --with_openssl=true
  xmake -a
  ```

- build libco

  ```sh
  xmake build libco     # build libco only
  xmake -b libco        # same as above
  ```

- build and run unitest code

  [co/unitest](https://github.com/idealvin/co/tree/master/unitest) contains some unit test codes, which are used to check the correctness of the functionality of libco.

  ```sh
  xmake build unitest    # build can be abbreviated as -b
  xmake run unitest -a   # run all unit tests
  xmake r unitest -a     # same as above
  xmake r unitest -os    # run unit test os
  xmake r unitest -json  # run unit test json
  ```

- build and run test code

  [co/test](https://github.com/idealvin/co/tree/master/test) contains some test codes. You can easily add a source file like `xxx.cc` in the directory `co/test` or its subdirectories, and then run `xmake build xxx` to build it.

  ```sh
  xmake build flag             # test/flag.cc
  xmake build log              # test/log.cc
  
  xmake r flag -xz             # test flag
  xmake r log                  # test log
  xmake r log -cout            # also log to terminal
  xmake r log -perf            # performance test
  ```

- build gen

  ```sh
  # It is recommended to put gen in the system directory (e.g. /usr/local/bin/).
  xmake build gen
  cp gen /usr/local/bin/
  gen hello_world.proto
  ```

  Proto file format can refer to [hello_world.proto](https://github.com/idealvin/co/blob/master/test/so/rpc/hello_world.proto).

- Installation

  ```sh
  # Install header files, libco, gen by default.
  xmake install -o pkg         # package related files to the pkg directory
  xmake i -o pkg               # the same as above
  xmake install -o /usr/local  # install to the /usr/local directory
  ```

### cmake

[izhengfan](https://github.com/izhengfan) helped provide cmake support:
- By default, only `libco` and `gen` are build.
- The library files are in the `build/lib` directory, and the executable files are in the `build/bin` directory.
- You can use `BUILD_ALL` to compile all projects.
- You can use `CMAKE_INSTALL_PREFIX` to specify the installation directory.

- Build libco and gen by default
  ```sh
  mkdir build && cd build
  cmake ..
  make -j8
  ```

- Build all projects
  ```sh
  mkdir build && cd build
  cmake .. -DBUILD_ALL=ON -DCMAKE_INSTALL_PREFIX=pkg
  make -j8
  make install
  ```

- Build with libcurl & openssl (libcurl, zlib, openssl 1.1.0 or above required)
  ```sh
  mkdir build && cd build
  cmake .. -DBUILD_ALL=ON -DWITH_LIBCURL=ON -DCMAKE_INSTALL_PREFIX=pkg
  make -j8
  make install
  ```

### Build from Docker

```
docker build -t co:v2.0.0 .
docker run -itd -v $(pwd):/home/co/ co:v2.0.0
docker exec -it ${CONTAINER_ID} bash    #replace with true CONTAINER_ID

# execute the following command in docker
cd /home/co && mkdir build && cd build
cmake .. -DBUILD_ALL=ON -DWITH_LIBCURL=ON
make -j8
```


## License

The MIT license. co contains codes from some other projects, which have their own licenses, see details in [LICENSE.md](https://github.com/idealvin/co/blob/master/LICENSE.md).


## Special thanks

- The code of [co/context](https://github.com/idealvin/co/tree/master/src/co/context) is from [tbox](https://github.com/tboox/tbox) by [ruki](https://github.com/waruqi), special thanks!
- The English reference documents of co are translated by [Leedehai](https://github.com/Leedehai) (1-10), [daidai21](https://github.com/daidai21) (11-15) and [google](https://translate.google.cn/), special thanks!
- [ruki](https://github.com/waruqi) has helped to improve the xmake building scripts, thanks in particular!
- [izhengfan](https://github.com/izhengfan) provided cmake building scripts, thank you very much!


## Donate

Goto the [Donate](https://www.yuque.com/idealvin/co/entqmb) page.
