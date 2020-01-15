# Documents for C++ base library CO

## Compile

[Xmake](https://github.com/xmake-io/xmake) is recommended for compiling the `CO` project. ~~[Scons](https://scons.org/)~~, ~~[vs project](https://visualstudio.microsoft.com/)~~ may not be supported in the future.

[izhengfan](https://github.com/izhengfan) has helped to provide cmake support. If you need to compile with cmake, please refer to [here](#compile-with-cmake).

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

- installation

  ```sh
  # Install header files, libbase, rpcgen by default.
  xmake install -o pkg         # Package related files to the pkg directory.
  xmake install -o /usr/local  # Install to the /usr/local directory.
  ```


### compile with cmake

- I know nothing about cmake. If you have any problem on compiling with cmake, please @[izhengfan](https://github.com/izhengfan) on github.

- Build libbase and rpcgen

  On the Unix system command line, use `cmake/make` to build:
  
  ```sh
  cd co
  mkdir cmake-build
  cd cmake-build
  cmake ..
  make
  ```

  For example, if you use cmake gui under Windows, set the source directory and build directory of co, click config, generate, and then build the generated Visual Studio solution.

  After the build is complete, the base library file is generated under `co/lib`, and the `rgcgen` executable file is generated under `co/build`.

- Build test and unitest

  The test and unitest are not built by default. To enable them, add two parameter settings in cmake:

  ```sh
  cmake .. -DBUILD_TEST=ON -DBUILD_UNITEST=ON
  ```

  The rest of the commands are the same as the previous one. For example, on the Windows cmake gui, check `BUILD_TEST` and `BUILD_UNITEST`, then click config and generate again, and reload the Visual Studio solution and build. After the build is complete, a test executable is generated under `co/build`.

- Install and use co libraries

  On the Unix command line, after `make` is complete, you can install:

  ```sh
  # At this point we are still in the co/cmake-build directory
  make install
  ```

  This command copies the `co/base` header files, library files, and the rpcgen executable to the appropriate subdirectories under the installation directory. The default installation location under Linux is `/usr/local/`, so `sudo` permission may be required when `make install`.

  To change the installation location, set the `CMAKE_INSTALL_PREFIX` parameter when cmake:

  ```sh
  cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local/
  ```

  For example, on the Windows cmake gui, set `CMAKE_INSTALL_PREFIX` to your desired installation path, and then build the `INSTALL` project in the rebuilt Visual Studio solution.

  After the installation is complete, you can use the `co` library in another project. An example CMakeLists.txt is as follows:

  ```cmake
  # @file CMakeLists.txt
  project(use_co)
  find_package(co REQUIRED)
  include_directories(${co_INCLUDE_DIR})
  add_executable(use_co main.cpp)
  target_link_libraries(use_co ${co_LIBS})
  ```

  ```cpp
  // @file main.cpp
  #include "base/flag.h"
  #include "base/log.h"

  int main(int argc, char** argv) {
      flag::init(argc, argv);
      log::init();
  }
  ```

  We define several equivalent variables to represent the include path (`co_INCLUDE_DIR`, `co_INCLUDE_DIRS`), whichever is used; we define several equivalent variables to represent the co library (`co_LIBS`, `co_LIBRARY`, `co_LIBRARIES`), link to either.

  If you can't find `coConfig.cmake` when you make cmake, you need to specify the `co_DIR` path manually. Assuming you just installed co into `/usr/local/`, add `-Dco_DIR=/usr/local/lib/cmake/co` after the `cmake` command, or set `co_DIR` in cmake gui to the `lib/cmake/co` sub-path under the co installation path.

  In addition, if the bin subpath (such as `/usr/local/bin`) under the co installation path is already in the environment variable, you can use the rpcgen command directly on the command line.
