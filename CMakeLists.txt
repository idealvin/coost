cmake_minimum_required(VERSION 3.10)
project(co)

if(WIN32)
    if (NOT MSVC)
        message(FATAL_ERROR, "msvc required on windows")
    endif()
    enable_language(C CXX ASM_MASM)
else()
    enable_language(C CXX ASM)
endif()

option(HAS_LIBCURL "Whether enable the LIBCURL" OFF)
if(HAS_LIBCURL)
    message(STATUS "LIBCURL enable for HTTP client")
    add_definitions(-DHAS_LIBCURL)
    find_package(CURL REQUIRED)
    if(CURL_FOUND)
        include_directories(${CURL_INCLUDE_DIR})
        link_directories(${CURL_LIBRARIES})
        message(STATUS "Curl Found: ${CURL_VERSION_STRING} ${CURL_LIBRARIES} ${CURL_LINK_LIBRARIES}!")
    else()
        message(FATAL_ERROR "Could not find the CURL library and development files.")
    endif()
endif()

set(CMAKE_CXX_STANDARD 11)
option(BUILD_SHARED_LIBS "Let us build static libs" OFF)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/bin)

include_directories(include)

if(UNIX)
    add_compile_options(-Wall -O2 -g3 -Wno-sign-compare -Wno-class-memaccess)
    if(APPLE)
        add_compile_options(-fno-pie)
    endif()
endif()

if(MSVC)
    add_compile_options(/W4 /fp:fast /EHsc)
    add_link_options(/SAFESEH:NO)
endif()

add_subdirectory(src)

option(BUILD_ALL "To build All" OFF)
option(BUILD_GEN "To build gen" ON)
option(BUILD_UNITEST "To build unitest" OFF)
option(BUILD_TEST "To build test" OFF)

if(BUILD_ALL)
    message("To build all")
    add_subdirectory(gen)
    add_subdirectory(unitest)
    add_subdirectory(test)
elseif(BUILD_GEN)
    message("To build gen")
    add_subdirectory(gen)
elseif(BUILD_UNITEST)
    message("To build unitest")
    add_subdirectory(unitest)
elseif(BUILD_TEST)
    message("To build test")
    add_subdirectory(test)
endif()

install(
    DIRECTORY include/co
    DESTINATION include
)
