# Build co library
file(GLOB_RECURSE CO_SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

if(MSVC)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64|AARCH64)$")
        set(ASM_FILES co/context/context_arm64.asm)
        set(ASM_LANG ASM_MARMASM)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(ASM_FILES co/context/context_x86.asm)
        set(ASM_LANG ASM_MASM)
    else()
        set(ASM_FILES co/context/context_x64.asm)
        set(ASM_LANG ASM_MASM)
    endif()

    enable_language(${ASM_LANG})
    set_property(SOURCE ${ASM_FILES} PROPERTY LANGUAGE ${ASM_LANG})
else()
    set(ASM_FILES co/context/context.S)
endif()
list(APPEND CO_SRC_FILES ${ASM_FILES})

if(WIN32)
    list(APPEND CO_SRC_FILES
        StackWalker.cpp
    )
endif()

add_library(co ${CO_SRC_FILES})

target_include_directories(co
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(co PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(WITH_BACKTRACE)
    target_compile_definitions(co PRIVATE WITH_BACKTRACE)
    target_link_libraries(co PUBLIC backtrace)
endif()

if(DEBUG_COROUTINE)
    target_compile_definitions(co PRIVATE CO_DEBUG_COROUTINE)
endif()

target_compile_features(co PUBLIC cxx_std_17)

if(FPIC)
    set_target_properties(co PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

find_package(Threads REQUIRED)
target_link_libraries(co
    PUBLIC Threads::Threads
    PRIVATE ${CMAKE_DL_LIBS}
)

if(WIN32)
    target_compile_definitions(co
        PRIVATE
            WIN32_LEAN_AND_MEAN
            _WINSOCK_DEPRECATED_NO_WARNINGS
    )
    target_link_libraries(co PUBLIC ws2_32)
endif()

if(APPLE)
    target_include_directories(co SYSTEM PRIVATE "/usr/local/include")
    target_link_directories(co PUBLIC "/usr/local/lib")
endif()

# Installation

## library & public headers
install(
    TARGETS co
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
