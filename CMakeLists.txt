cmake_minimum_required(VERSION 3.10)
project(co)

if(MSVC)
    enable_language(C CXX ASM_MASM)
else()
    enable_language(C CXX ASM)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/bin)


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

include_directories(include)
add_subdirectory(src)

if(BUILD_ALL)
    add_subdirectory(gen)
    add_subdirectory(unitest)
    add_subdirectory(test)
endif()

install(
    DIRECTORY include/co
    DESTINATION include
)
