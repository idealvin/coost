file(GLOB_RECURSE CC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

include(CheckIncludeFiles)
check_include_files(execinfo.h HAS_EXECINFO)
if(HAS_EXECINFO)
    add_definitions(-DHAS_EXECINFO_H)
endif()

if(UNIX)
    add_library(co
        ${CC_FILES}
        co/context/context.S
    )
endif()

if(WIN32)
    set(ASM_FILES co/context/context_x86.asm)
    if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
        set(ASM_FILES co/context/context_x64.asm)
    endif()

    set_property(SOURCE ${ASM_FILES} PROPERTY LANGUAGE ASM_MASM)

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
endif()

install(
    TARGETS co
    LIBRARY DESTINATION lib   # shared lib installed to   ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE DESTINATION lib   # static lib installed to   ${CMAKE_INSTALL_PREFIX}/lib
)
