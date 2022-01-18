cmake_minimum_required(VERSION 3.12)

if(POLICY CMP0090)
    cmake_policy(SET CMP0090 NEW)
endif()

project(co VERSION 2.0.3)

if(MSVC)
    enable_language(C CXX ASM_MASM)
else()
    enable_language(C CXX ASM)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(MSVC)
    add_compile_options(/W4 /fp:fast /EHsc)
    add_link_options(/SAFESEH:NO)
else()
    add_compile_options(-Wall -O2 -g -Wno-sign-compare -Wno-class-memaccess -Wno-strict-aliasing)
    if(APPLE)
        add_compile_options(-fno-pie)
    endif()
endif()


# build with libcurl (openssl & zlib also required)
option(WITH_LIBCURL "build with libcurl" OFF)

# build with openssl 1.1.0+
option(WITH_OPENSSL "build with openssl" OFF)

# build with -fPIC
option(FPIC "build with -fPIC" OFF)

# build all projects (libco, gen, test, unitest)
option(BUILD_ALL "Build all projects" OFF)

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
