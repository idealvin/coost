cmake_minimum_required(VERSION 3.1)
project(co)
if(MSVC)
  enable_language(C CXX ASM_MASM)
else()
  enable_language(C CXX ASM)
endif()

set(CMAKE_CXX_STANDARD 11)
option(BUILD_SHARED_LIBS "Let us build static libs" OFF)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

set(CMAKE_DEBUG_POSTFIX "d")

include_directories(include)

if(UNIX)
  add_compile_options(-Wall -O2 -g3 -lpthread -ldl)
  if(APPLE)
    add_compile_options(-fno-pie)
  endif()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fPIC)
  endif()
endif()

if(MSVC)
  add_compile_options(/W4 /fp:fast /EHsc)
  add_link_options(/SAFESEH:NO)
endif()

add_subdirectory(src)

option(BUILD_RPCGEN "To build gen" ON)
if(BUILD_RPCGEN)
  message("To build gen" )
  add_subdirectory(gen)
endif()

option(BUILD_UNITEST "To build unitest" OFF)
if(BUILD_UNITEST)
  message("To build unitest" )
  add_subdirectory(unitest)
endif()

option(BUILD_TEST "To build test" OFF)
if(BUILD_TEST)
  message("To build test" )
  add_subdirectory(test)
endif()


install(DIRECTORY include/co
  DESTINATION include
  )