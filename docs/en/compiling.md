# Documents for C++ base library CO

## Compiling

[Xmake](https://github.com/xmake-io/xmake) is recommended for compiling the `CO` project.

[izhengfan](https://github.com/izhengfan) has helped to provide cmake support. If you need to compile with cmake, please refer to [here](#compile-with-cmake).

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


### compile with cmake

- I know nothing about cmake. If you have any problem on compiling with cmake, please @[izhengfan](https://github.com/izhengfan) on github.

- Build libco and gen

  On the Unix system command line, use `cmake/make` to build:
  
  ```sh
  cd co
  mkdir cmake-build
  cd cmake-build
  cmake ..
  make
  ```

  If using cmake gui under Windows, set the source directory and build directory of co, click `Config`, `Generate`, and then build the generated Visual Studio solution.

  After the building is completed, the base library file is generated in `co/lib`, and the `gen` executable file is generated in `co/build`.

- Build test and unitest

  The `test` and `unitest` are not built by default. To enable them, add two parameter settings in cmake:

  ```sh
  cmake .. -DBUILD_TEST=ON -DBUILD_UNITEST=ON
  ```

  The rest of the commands are the same as the previous one. If using the Windows cmake gui, tick `BUILD_TEST` and `BUILD_UNITEST`, then click `Config` and `Generate` again, and reload the Visual Studio solution and build. After the building is completed, a test executable is generated under `co/build`.

- Install and use co libraries

  On the Unix command line, after `make` is completed, you can install:

  ```sh
  # At this point we are still in the co/cmake-build directory
  make install
  ```

  This command copies the `co/base` header files, library files, and the gen executable to the appropriate subdirectories under the installation directory. The default installation location under Linux is `/usr/local/`, so `sudo` permission may be required when `make install`.

  To change the installation location, set the `CMAKE_INSTALL_PREFIX` parameter when cmake:

  ```sh
  cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local/
  ```

  If using the Windows cmake gui, set `CMAKE_INSTALL_PREFIX` to your desired installation path, and then build the `INSTALL` project in the rebuilt Visual Studio solution.

  After the installation is completed, you can use the `co` library in another project. An example CMakeLists.txt is as follows:

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
  #include "co/flag.h"
  #include "co/log.h"

  int main(int argc, char** argv) {
      flag::init(argc, argv);
      log::init();
  }
  ```

  We define several equivalent variables to represent the include path (`co_INCLUDE_DIR`, `co_INCLUDE_DIRS`), whichever is usable; we define several equivalent variables to represent the co library (`co_LIBS`, `co_LIBRARY`, `co_LIBRARIES`), link to either one of them.

  If you can't find `coConfig.cmake` when you make cmake, you need to specify the `co_DIR` path manually. Assuming you just installed `co` into `/usr/local/`, add `-Dco_DIR=/usr/local/lib/cmake/co` after the `cmake` command, or set `co_DIR` in cmake gui to the `lib/cmake/co` sub-path under the `co` installation path.

  In addition, if the bin subpath (such as `/usr/local/bin`) under the co installation path is already in the environmental PATH, you can use the `gen` command directly on the command line.
