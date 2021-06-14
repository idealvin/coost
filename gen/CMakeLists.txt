file(GLOB SRC_FILES *.cc)

add_executable(gen ${SRC_FILES})

list(APPEND LIBS_TO_LINK co)
if(UNIX)
    list(APPEND LIBS_TO_LINK pthread dl)
endif()

if(HAS_LIBCURL)
    list(APPEND LIBS_TO_LINK curl crypto ssl pthread dl)
endif()

target_link_libraries(gen
    ${LIBS_TO_LINK}
)

install(
    TARGETS gen
    RUNTIME DESTINATION bin
)
