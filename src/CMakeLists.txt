file(GLOB_RECURSE CC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

if(UNIX)
    add_library(co
        ${CC_FILES}
        co/context/context.S
    )
endif()

if(WIN32)
    file(GLOB_RECURSE CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

    set(ASM_FILES co/context/context_x86.asm)
    if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
        set(ASM_FILES co/context/context_x64.asm)
    endif()

    set_property(SOURCE ${ASM_FILES} PROPERTY LANGUAGE ASM_MASM)

    add_library(co
        ${CC_FILES}
        ${CPP_FILES}
        ${ASM_FILES}
    )
endif()

install(
    TARGETS co
    LIBRARY DESTINATION lib   # shared lib installed to   ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE DESTINATION lib   # static lib installed to   ${CMAKE_INSTALL_PREFIX}/lib
)
