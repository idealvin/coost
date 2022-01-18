# Build co library
file(GLOB_RECURSE CO_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

if(MSVC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(ASM_FILES co/context/context_x86.asm)
    else()
        set(ASM_FILES co/context/context_x64.asm)
    endif()
    set_property(SOURCE ${ASM_FILES} PROPERTY LANGUAGE ASM_MASM)
else()
    set(ASM_FILES co/context/context.S)
endif()
list(APPEND CO_SRC_FILES ${ASM_FILES})

if(WIN32)
    list(APPEND CO_SRC_FILES
        log/StackWalker.cpp
        co/detours/creatwth.cpp
        co/detours/detours.cpp
        co/detours/image.cpp
        co/detours/modules.cpp
        co/detours/disasm.cpp
    )
endif()

add_library(co ${CO_SRC_FILES})
add_library(cocoyaxi::co ALIAS co)

target_include_directories(co
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(co PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(WITH_LIBCURL)
    find_package(CURL REQUIRED)
    target_compile_definitions(co PRIVATE HAS_LIBCURL)
    target_link_libraries(co PRIVATE CURL::libcurl)
endif()
if(WITH_LIBCURL OR WITH_OPENSSL)
    find_package(OpenSSL 1.1.0 REQUIRED)
    target_compile_definitions(co PRIVATE HAS_OPENSSL)
    target_link_libraries(co PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

target_compile_features(co PUBLIC cxx_std_11)

if(FPIC)
    set_target_properties(co PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

find_package(Threads REQUIRED)
target_link_libraries(co PUBLIC Threads::Threads)

if(UNIX AND NOT APPLE)
    target_link_libraries(co PRIVATE dl)
endif()

if(WIN32)
    if(MSVC)
        if(BUILD_SHARED_LIBS)
            target_compile_definitions(co PRIVATE BUILDING_CO_DLL)
            set(USING_CO_DLL 1)
        else()
            set(USING_CO_DLL 0)
        endif()
        configure_file(
            "${PROJECT_SOURCE_DIR}/include/co/config.h.in"
            "${PROJECT_SOURCE_DIR}/include/co/config.h"
        )
    endif()
    target_compile_definitions(co PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(co PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)
    target_link_libraries(co PUBLIC ws2_32)
else()
    include(CheckIncludeFiles)
    include(CheckIncludeFileCXX)
    check_include_files(backtrace.h HAS_BACKTRACE)
    check_include_file_cxx(cxxabi.h HAS_CXXABI)
    if(HAS_BACKTRACE)
        target_compile_definitions(co PRIVATE HAS_BACKTRACE_H)
        target_link_libraries(co PUBLIC backtrace)
    endif()
    if(HAS_CXXABI)
        target_compile_definitions(co PRIVATE HAS_CXXABI_H)
    endif()
endif()

# Installation

## library & public headers
install(
    TARGETS co
    EXPORT cocoyaxiExport
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/include/co
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

## license
install(
    FILES ${PROJECT_SOURCE_DIR}/LICENSE.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

## pkgconfig file
if(WIN32)
    string(APPEND CO_PKG_EXTRA_LIBS " -lws2_32")
else()
    if(UNIX AND NOT APPLE)
        string(APPEND CO_PKG_EXTRA_LIBS " -lpthread")
        if(NOT BUILD_SHARED_LIBS)
            string(APPEND CO_PKG_EXTRA_LIBS " -ldl")
        endif()
    endif()
    if(HAS_BACKTRACE)
        string(APPEND CO_PKG_EXTRA_LIBS " -lbacktrace")
    endif()
endif()
if(WITH_LIBCURL)
    string(APPEND CO_PKG_REQUIRES " libcurl")
endif()
if(WITH_LIBCURL OR WITH_OPENSSL)
    string(APPEND CO_PKG_REQUIRES " openssl >= 1.1.0")
endif()

configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/cocoyaxi.pc.in
    cocoyaxi.pc
    @ONLY
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/cocoyaxi.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

## CMake config file
set(CO_CMAKE_CONFIG_DEPS "find_dependency(Threads)\n")
if(WITH_LIBCURL)
    string(APPEND CO_CMAKE_CONFIG_DEPS "find_dependency(CURL)\n")
endif()
if(WITH_LIBCURL OR WITH_OPENSSL)
    string(APPEND CO_CMAKE_CONFIG_DEPS "find_dependency(OpenSSL 1.1.0)\n")
endif()

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/cocoyaxiConfig.cmake.in
    cocoyaxiConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cocoyaxi
)

write_basic_package_version_file(
    cocoyaxiConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

export(
    EXPORT cocoyaxiExport
    NAMESPACE cocoyaxi::
    FILE cocoyaxiTargets.cmake
)

install(
    EXPORT cocoyaxiExport
    NAMESPACE cocoyaxi::
    FILE cocoyaxiTargets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cocoyaxi
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/cocoyaxiConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cocoyaxiConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cocoyaxi
)

export(PACKAGE cocoyaxi)
