cmake_minimum_required(VERSION 3.12)

if(POLICY CMP0090)
    cmake_policy(SET CMP0090 NEW)
endif()

project(coost VERSION 3.0.0)

if(MSVC)
    enable_language(C CXX ASM_MASM)
else()
    enable_language(C CXX ASM)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(MSVC)
    add_compile_options(/fp:fast /EHsc)
    add_link_options(/SAFESEH:NO)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
else()
    add_compile_options(-Wall -O2 -g -Wno-sign-compare -Wno-strict-aliasing)
    if(APPLE)
        add_compile_options(-fno-pie)
    endif()
endif()


# build with openssl 1.1.0+
option(WITH_OPENSSL "build with openssl" OFF)

# build with libcurl (openssl & zlib also required)
option(WITH_LIBCURL "build with libcurl" OFF)

# build with libbacktrace
option(WITH_BACKTRACE "build with libbacktrace" OFF)

# build with -fPIC
option(FPIC "build with -fPIC" OFF)

# build all projects (libco, gen, test, unitest)
option(BUILD_ALL "Build all projects" OFF)

option(DISABLE_HOOK "disable hooks for system APIs" OFF)

# specify the value of L1 cache line size, 64 by default
set(CACHE_LINE_SIZE "64" CACHE STRING "set value of L1 cache line size")

# vs runtime, use MT
if(MSVC)
    option(STATIC_VS_CRT "use /MT or /MTd" OFF)

    if(STATIC_VS_CRT)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    endif()
endif()

add_subdirectory(src)

if(BUILD_ALL)
    enable_testing()
    add_subdirectory(gen)
    add_subdirectory(unitest)
    add_subdirectory(test)
endif()
