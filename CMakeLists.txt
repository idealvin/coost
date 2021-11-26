cmake_minimum_required(VERSION 3.10)
project(co)

include (utils.cmake)

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
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wno-builtin-macro-redefined")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-builtin-macro-redefined")
    add_compile_options(/W4 /fp:fast /EHsc)
    add_link_options(/SAFESEH:NO)
else()
    add_definitions(-Wno-builtin-macro-redefined)
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
    option(STATIC_VS_CRT "use /MT or /MTd" ON)

    if(STATIC_VS_CRT)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    endif()


    option(CURL_STATICLIB "use static libcurl" ON)
    if(CURL_STATICLIB)
        add_definitions("-DCURL_STATICLIB")
    endif()
endif()


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
