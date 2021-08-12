file(GLOB_RECURSE CC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

# source files
if(WIN32)
    set(ASM_FILES co/context/context_x64.asm)
    if(MSVC)
        if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
            set(ASM_FILES co/context/context_x86.asm)
        endif()
        set_property(SOURCE ${ASM_FILES} PROPERTY LANGUAGE ASM_MASM)
    else()
        set(ASM_FILES co/context/context.S)
    endif()

    add_library(co
        ${CC_FILES}
        ${ASM_FILES}
        __/StackWalker.cpp
        co/detours/creatwth.cpp
        co/detours/detours.cpp
        co/detours/image.cpp
        co/detours/modules.cpp
        co/detours/disasm.cpp
    )
else()
    add_library(co
        ${CC_FILES}
        co/context/context.S
    )
endif()


include(CheckIncludeFiles)
check_include_files(execinfo.h HAS_EXECINFO)
if(HAS_EXECINFO)
    target_compile_definitions(co PRIVATE HAS_EXECINFO_H)
endif()


if(WITH_LIBCURL)
    find_package(OpenSSL 1.1.0 REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(CURL REQUIRED)
    if(CURL_FOUND)
        message(STATUS "found openssl, include: ${OPENSSL_INCLUDE_DIR}, lib: ${OPENSSL_LIBRARIES}")
        message(STATUS "found zlib ${ZLIB_VERSION_STRING}, include: ${ZLIB_INCLUDE_DIR}, lib: ${ZLIB_LIBRARIES}")
        message(STATUS "found libcurl ${CURL_VERSION_STRING}, include: ${CURL_INCLUDE_DIR}, lib: ${CURL_LIBRARIES}")
        target_compile_definitions(co PRIVATE CO_SSL)
        target_compile_definitions(co PRIVATE HAS_OPENSSL)
        target_compile_definitions(co PRIVATE HAS_LIBCURL)
        target_include_directories(co PRIVATE ${CURL_INCLUDE_DIR})
        target_include_directories(co PRIVATE ${OPENSSL_INCLUDE_DIR})
        target_include_directories(co PRIVATE ${ZLIB_INCLUDE_DIR})
        target_link_libraries(co PRIVATE ${CURL_LIBRARIES})
        target_link_libraries(co PRIVATE ${ZLIB_LIBRARIES})
        target_link_libraries(co PRIVATE ${OPENSSL_LIBRARIES})
    endif()
elseif(WITH_OPENSSL)
    find_package(OpenSSL 1.1.0 REQUIRED)
    if(OPENSSL_FOUND)
        message(STATUS "found openssl, include: ${OPENSSL_INCLUDE_DIR}, lib: ${OPENSSL_LIBRARIES}")
        target_compile_definitions(co PRIVATE CO_SSL)
        target_compile_definitions(co PRIVATE HAS_OPENSSL)
        target_include_directories(co PRIVATE ${OPENSSL_INCLUDE_DIR})
        target_link_libraries(co PRIVATE ${OPENSSL_LIBRARIES})
    endif()
endif()


if(WIN32)
    target_compile_definitions(co PUBLIC WIN32_LEAN_AND_MEAN)
    if(not MSVC)
        target_compile_definitions(co PRIVATE _FILE_OFFSET_BITS=64)
        target_link_libraries(co PUBLIC ws2_32)
    endif()
else()
    target_compile_definitions(co PRIVATE _FILE_OFFSET_BITS=64)
    if(NOT ANDROID)
        target_link_libraries(co PUBLIC pthread)
        target_link_libraries(co PRIVATE dl)
    endif()
endif()


install(
    TARGETS co
    LIBRARY DESTINATION lib   # shared lib installed to   ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE DESTINATION lib   # static lib installed to   ${CMAKE_INSTALL_PREFIX}/lib
)
